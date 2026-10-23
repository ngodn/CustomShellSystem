"""Cook the verified Profane Proxima ghost material set with Windows UE 5.6.1 (copy of cook_peony_materials.py)."""
import hashlib
import argparse
import json
from pathlib import Path
import shutil
import subprocess


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    workspace = Path('/home/eins0fx/development/mods/msII')
    root = workspace / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
    work = Path('/mnt/eins0fxE/CSS-Development/profane-astral1')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, default=work)
    parser.add_argument('--author-exit', default='author3.exit.json')
    parser.add_argument('--verify-exit', default='verify1.exit.json')
    parser.add_argument('--output-name', default='windows-cook1')
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    if Path(args.output_name).name != args.output_name:
        raise ValueError('Cook output must be inside the selected work directory')
    output = work / args.output_name
    if output.exists():
        raise FileExistsError(output)
    verified = json.loads((work / 'verified.json').read_text())
    if not verified['passed'] or verified['authored_sha256'] != sha(work / 'authored.json'):
        raise ValueError('Fresh-editor readback is missing or stale')
    for name in (args.author_exit, args.verify_exit):
        if Path(name).name != name:
            raise ValueError('Exit receipts must be inside the selected work directory')
        if json.loads((work / name).read_text())['returncode'] != 0:
            raise ValueError('Editor process did not exit successfully')
    authored = json.loads((work / 'authored.json').read_text())
    protected = dict(authored['sources'])
    for package, expected in authored['packages'].items():
        source = work / 'editor/Content' / (package.removeprefix('/Game/') + '.uasset')
        protected[str(source)] = expected
    if any(sha(Path(p)) != checksum for p, checksum in protected.items()):
        raise ValueError('Authoring inputs changed')
    ug = root.parent / 'CSS_UnholyGenessa_eins0fx_P'
    engine = ug / 'work/ue56'
    version = json.loads((engine / 'Engine/Build/Build.version').read_text())
    if tuple(version[k] for k in ('MajorVersion', 'MinorVersion', 'PatchVersion', 'Changelist')) != (5, 6, 1, 44394996):
        raise ValueError('Unexpected Windows engine build')
    wrapper = ug / 'tools/docker-local.sh'
    image = subprocess.check_output(['bash', str(wrapper), 'image', 'inspect', 'unholy-wine:11.7',
                                    '--format', '{{.Id}}'], text=True).strip()
    project, out = output / 'project', output / 'out'
    (project / 'Config').mkdir(parents=True)
    out.mkdir()
    (project / 'CSSShared.uproject').write_text(json.dumps(dict(FileVersion=3, EngineAssociation='5.6',
        Plugins=[dict(Name='AndroidFileServer', Enabled=False)]), indent=2) + '\n')
    (project / 'Config/DefaultEngine.ini').write_text(
        '[/Script/EngineSettings.GameMapsSettings]\nGameDefaultMap=\nServerDefaultMap=\nEditorStartupMap=\n'
        '[/Script/WindowsTargetPlatform.WindowsTargetSettings]\nDefaultGraphicsRHI=DefaultGraphicsRHI_DX12\n'
        '!D3D12TargetedShaderFormats=ClearArray\n+D3D12TargetedShaderFormats=PCD3D_SM6\n'
        '!D3D11TargetedShaderFormats=ClearArray\n+D3D11TargetedShaderFormats=PCD3D_SM5\n')
    (project / 'Config/DefaultGame.ini').write_text(
        '[/Script/UnrealEd.ProjectPackagingSettings]\nbShareMaterialShaderCode=False\n'
        'bSharedMaterialNativeLibraries=False\nbUseIoStore=False\n')
    for package in authored['packages']:
        relative = Path('Content') / (package.removeprefix('/Game/') + '.uasset')
        destination = project / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(work / 'editor' / relative, destination)
    script = workspace / 'CustomShellSystem/tools/shared-assets/cook-shared-inner.sh'
    def mount_source(path):
        # This workspace daemon binds its private state over /mnt. It keeps
        # the original drive mounts reachable through host-mnt in its namespace.
        path = path.resolve()
        if path.is_relative_to('/mnt'):
            return ug / 'work/docker1/host-mnt' / path.relative_to('/mnt')
        return path
    command = ['bash', str(wrapper), 'run', '--rm', '--init', '--user', '0:0',
        '--name', work.name + '-' + args.output_name, '--network', 'none', '--shm-size', '1g',
        '--mount', f'type=bind,src={engine},dst=/ue,readonly',
        '--mount', f'type=bind,src={mount_source(project)},dst=/project',
        '--mount', f'type=bind,src={mount_source(out)},dst=/out',
        '--mount', f'type=bind,src={script},dst=/cook.sh,readonly', image, 'bash', '/cook.sh']
    (output / 'inputs.json').write_text(json.dumps(dict(files=protected, image=image, engine=version,
        command=command, inner_script_sha256=sha(script), recipe_sha256=sha(Path(__file__))), indent=2) + '\n')
    with (output / 'console.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    unchanged = all(sha(Path(p)) == checksum for p, checksum in protected.items())
    (output / 'exit.json').write_text(json.dumps(dict(returncode=result.returncode,
        sources_unchanged=unchanged), indent=2) + '\n')
    result.check_returncode()
    if not unchanged:
        raise ValueError('Source preservation failed')
    shader_failures = [line for line in (output / 'console.log').read_text(errors='replace').splitlines()
                       if 'Failed to compile Material' in line or "doesn't have a valid ShaderMap" in line]
    (output / 'shader-check.json').write_text(json.dumps(dict(passed=not shader_failures,
        failures=shader_failures), indent=2) + '\n')
    if shader_failures:
        raise ValueError('Cook exited zero but emitted invalid material shaders; see shader-check.json')


if __name__ == '__main__':
    main()

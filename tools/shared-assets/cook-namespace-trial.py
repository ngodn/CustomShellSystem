"""Cook verified namespaced assets with the existing Windows UE 5.6.1 toolchain."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def private_storage(path):
    resolved = path.resolve()
    work = (Path(__file__).resolve().parents[2] / 'work').resolve()
    return (resolved.is_relative_to(Path('/home')) or
            (resolved != work and resolved.is_relative_to(work)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--docker-wrapper', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--asset-root', default='/Game/CSS/eins0fx/Shared/Astral/')
    parser.add_argument('--expected-assets', type=int, default=30)
    parser.add_argument('--renderer-config', type=Path, help='Pinned original RendererSettings to preserve shader variants')
    parser.add_argument('--ddc-seed', type=Path, help='Copy a previous private cook cache')
    args = parser.parse_args()
    editor, output = args.editor.resolve(strict=True), args.output.resolve()
    engine, wrapper = args.engine.resolve(strict=True), args.docker_wrapper.resolve(strict=True)
    if output.exists() or not private_storage(output):
        raise ValueError('Use a new private output directory under /home or CSS work')
    if not (engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe').is_file():
        raise ValueError('Expected the existing Windows Unreal toolchain')
    version = json.loads((engine/'Engine/Build/Build.version').read_text())
    if tuple(version[key] for key in ('MajorVersion', 'MinorVersion', 'PatchVersion')) != (5, 6, 1):
        raise ValueError('This migration requires UE 5.6.1')
    if (not json.loads((editor/'reload.json').read_text())['passed'] or
            json.loads((editor/'reload-exit.json').read_text())['returncode'] != 0):
        raise ValueError('Editor reload must pass first')
    migrated = json.loads((editor/'migration.json').read_text())['assets']
    if (not args.asset_root.startswith('/Game/CSS/') or not args.asset_root.endswith('/') or
            '..' in args.asset_root.split('/') or args.expected_assets < 1):
        raise ValueError('Expected an authored asset directory and positive asset count')
    assets = [row for row in migrated if row['package'].startswith(args.asset_root)]
    if len(assets) != args.expected_assets:
        raise ValueError('Migrated asset count differs from the requested cook')
    inputs = json.loads((editor/'copied-inputs.json').read_text())
    for entry in inputs:
        if sha(Path(entry['path'])) != entry['sha256']:
            raise ValueError('Protected authoring input changed: '+entry['path'])
    renderer, renderer_source = '', None
    if args.renderer_config:
        source = args.renderer_config.resolve(strict=True)
        config = configparser.ConfigParser(interpolation=None)
        config.optionxform = str
        config.read(source)
        section = '/Script/Engine.RendererSettings'
        values = dict(config[section])
        if not values or any('\n' in value for value in values.values()):
            raise ValueError('Expected single-line renderer settings')
        renderer = '['+section+']\n'+''.join(key+'='+value+'\n' for key, value in values.items())
        renderer_source = dict(path=str(source), sha256=sha(source), settings=values)
    image = subprocess.check_output(['bash', str(wrapper), 'image', 'inspect', 'unholy-wine:11.7',
                                     '--format', '{{.Id}}'], text=True).strip()
    output.mkdir(parents=True)
    project, out = output/'project', output/'out'
    (project/'Config').mkdir(parents=True)
    out.mkdir()
    (project/'CSSShared.uproject').write_text(json.dumps(dict(FileVersion=3, EngineAssociation='5.6',
        Plugins=[dict(Name='AndroidFileServer', Enabled=False)]), indent=2)+'\n')
    (project/'Config/DefaultEngine.ini').write_text(
        renderer+'[/Script/EngineSettings.GameMapsSettings]\nGameDefaultMap=\nServerDefaultMap=\nEditorStartupMap=\n'
        '[/Script/WindowsTargetPlatform.WindowsTargetSettings]\nDefaultGraphicsRHI=DefaultGraphicsRHI_DX12\n'
        '!D3D12TargetedShaderFormats=ClearArray\n+D3D12TargetedShaderFormats=PCD3D_SM6\n'
        '!D3D11TargetedShaderFormats=ClearArray\n+D3D11TargetedShaderFormats=PCD3D_SM5\n')
    (project/'Config/DefaultGame.ini').write_text(
        '[/Script/UnrealEd.ProjectPackagingSettings]\nbShareMaterialShaderCode=False\n'
        'bSharedMaterialNativeLibraries=False\nbUseIoStore=False\n')
    protected = {}
    if renderer_source:
        protected[renderer_source['path']] = renderer_source['sha256']
    if args.ddc_seed:
        seed = args.ddc_seed.resolve(strict=True)
        if not private_storage(seed) or not seed.is_dir():
            raise ValueError('Expected an existing local private cache directory')
        shutil.copytree(seed, out/'ddc')
    for row in assets:
        relative = Path('Content') / (row['package'].removeprefix('/Game/')+'.uasset')
        source, target = editor/relative, project/relative
        if sha(source) != row['sha256']:
            raise ValueError('Migrated source changed: '+str(source))
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        protected[str(source)] = row['sha256']
    script = Path(__file__).with_name('cook-shared-inner.sh').resolve(strict=True)
    command = ['bash', str(wrapper), 'run', '--rm', '--init', '--user', '0:0',
        '--name', output.name, '--network', 'none', '--shm-size', '1g',
        '--mount', f'type=bind,src={engine},dst=/ue,readonly',
        '--mount', f'type=bind,src={project},dst=/project',
        '--mount', f'type=bind,src={out},dst=/out',
        '--mount', f'type=bind,src={script},dst=/cook.sh,readonly', image, 'bash', '/cook.sh']
    (output/'command.json').write_text(json.dumps(command, indent=2)+'\n')
    (output/'sources.json').write_text(json.dumps(dict(files=protected, image=image, engine=version,
        renderer=renderer_source, cache_seed=str(args.ddc_seed.resolve()) if args.ddc_seed else None,
        editor_receipts={name: sha(editor/name) for name in ['migration.json', 'reload.json', 'reload-exit.json']}), indent=2)+'\n')
    with (output/'console.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    unchanged = all(sha(Path(path)) == checksum for path, checksum in protected.items())
    (output/'exit.json').write_text(json.dumps(dict(returncode=result.returncode, sources_unchanged=unchanged))+'\n')
    result.check_returncode()
    if not unchanged:
        raise ValueError('Protected migrated inputs changed')
    print(f'Windows cook passed for {len(assets)} assets; independent payload/shader checks remain')


if __name__ == '__main__':
    main()

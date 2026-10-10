"""Cook canonical Peony and Astral candidates with explicit shipping ownership."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time


CSS = Path(__file__).resolve().parents[2]
EVE = CSS.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', choices=('peony-windows1', 'peony-windows4', 'peony-windows5', 'peony-windows6'), required=True)
    parser.add_argument('--single-thread', action='store_true',
                        help='Disable cooker workers for the observed vertex-attribute build crash')
    args = parser.parse_args()
    repaired = args.stage in ('peony-windows5', 'peony-windows6')
    lower_refit = args.stage == 'peony-windows6'
    meshes = EVE / 'work' / ('peony-cookprep4' if lower_refit else 'peony-cookprep3' if repaired else 'peony-cookprep1')
    astral = EVE / 'work/peony-astral2'
    output = EVE / 'work' / args.stage
    if output.exists():
        raise FileExistsError(output)
    if shutil.disk_usage(EVE).free < 12 * 1024**3:
        raise ValueError('Need 12 GiB free for the isolated Windows cook')
    for stage, phases in ((meshes, ('author', 'reload', 'dependencies')),
                          (astral, ('author', 'reload'))):
        for phase in phases:
            if read(stage / (phase + '-exit.json'))['exit_code'] != 0:
                raise ValueError('Failed editor phase: ' + str(stage / phase))
    gpu = EVE / 'work' / ('peony-clothgpu4' if lower_refit else 'peony-clothgpu3' if repaired else 'peony-clothgpu1')
    if not read(gpu / 'gpu-comparison.json')['passed'] or not read(astral / 'reloaded.json')['passed']:
        raise ValueError('GPU/reload verification missing')
    protected = {}
    for stage in (meshes, astral, gpu):
        receipt = stage / 'validation-receipt.json'
        data = read(receipt)
        for path, digest in data['files'].items():
            candidate = Path(path)
            if not candidate.is_absolute():
                candidate = stage / candidate
            if sha(candidate) != digest:
                raise ValueError('Validation input changed: ' + path)
        protected[str(receipt)] = sha(receipt)

    plan_path = meshes / 'cook-plan.json'
    plan = read(plan_path)
    groups = {key: dict(value) for key, value in plan['groups'].items()}
    if plan['counts'] != dict(cook_outfit=55, cook_shared=1, reuse_cooked_outfit=43,
                              reuse_cooked_shared=3, editor_only=7):
        raise ValueError('Unexpected canonical dependency inventory')
    protected[str(plan_path)] = sha(plan_path)
    sources = {}

    def add(package, path, expected, owner):
        if not package.startswith('/Game/') or '..' in package.split('/'):
            raise ValueError('Invalid package path')
        if package in sources or sha(path) != expected:
            raise ValueError('Duplicate or changed input: ' + package)
        sources[package] = dict(path=str(path), sha256=expected, owner=owner)
        protected[str(path)] = expected

    for owner, packages in groups.items():
        for package, digest in packages.items():
            add(package, meshes / 'editor/Content' / (package[6:] + '.uasset'), digest, owner)
    manifest = read(astral / 'authored.json')
    new_shared = [row['material'] for row in manifest['materials']]
    new_shared += [row['variant'] for row in manifest['variants']]
    new_shared += manifest['additional_shared_assets']
    if len(new_shared) != 9:
        raise ValueError('Expected four Astral parents, four variants and one mask texture')
    prefix = '/Game/CSS/eins0fx/Shared/Astral/Materials/'
    for object_path in new_shared:
        package = object_path.split('.')[0]
        if not package.startswith(prefix):
            raise ValueError('Astral ownership differs')
        path = astral / 'editor/Content' / (package[6:] + '.uasset')
        digest = sha(path)
        add(package, path, digest, 'cook_shared')
        groups['cook_shared'][package] = digest
    for name in ('Color', 'Normal', 'Linear'):
        package = prefix + 'T_Neutral' + name
        path = astral / 'editor/Content' / (package[6:] + '.uasset')
        digest = sha(path)
        add(package, path, digest, 'reuse_cooked_shared')
        groups['reuse_cooked_shared'][package] = digest
    if len(sources) != 121 or len(groups['cook_shared']) != 10:
        raise ValueError('Unexpected combined cook inventory')

    ug = EVE.parent / 'CSS_UnholyGenessa_eins0fx_P'
    engine, wrapper = ug / 'work/ue56', ug / 'tools/docker-local.sh'
    version_path = engine / 'Engine/Build/Build.version'
    version = read(version_path)
    if tuple(version[k] for k in ('MajorVersion', 'MinorVersion', 'PatchVersion', 'Changelist')) != (5, 6, 1, 44394996):
        raise ValueError('Wrong Windows UE build')
    image = subprocess.check_output(['bash', str(wrapper), 'image', 'inspect', 'unholy-wine:11.7',
                                     '--format', '{{.Id}}'], text=True).strip()
    project, out = output / 'project', output / 'out'
    project.mkdir(parents=True)
    out.mkdir()
    baseline = EVE / 'work/peony-character2/windows-cook1/project'
    descriptor_path = baseline / 'CSSShared.uproject'
    descriptor = read(descriptor_path)
    if descriptor.get('Modules') or any(p['Name'] == 'DeformerGraph' for p in descriptor['Plugins']):
        raise ValueError('Unexpected baseline project modules/plugins')
    descriptor['Plugins'].append(dict(Name='DeformerGraph', Enabled=True))
    (project / 'CSSShared.uproject').write_text(json.dumps(descriptor, indent=2) + '\n')
    shutil.copytree(baseline / 'Config', project / 'Config')
    for path in [descriptor_path, version_path, *sorted((baseline / 'Config').rglob('*'))]:
        if path.is_file():
            protected[str(path)] = sha(path)
    for package, row in sources.items():
        target = project / 'Content' / (package[6:] + '.uasset')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(row['path'], target)
    seed = EVE / 'work/peony-character2/windows-cook1/out/ddc'
    if repaired:
        previous = EVE / 'work' / ('peony-windows5' if lower_refit else 'peony-windows4')
        previous_exit = read(previous / 'exit.json')
        if (previous_exit['returncode'] != 0 or not previous_exit['sources_unchanged']
                or read(previous / 'inputs.json')['engine'] != version):
            raise ValueError('Previous Peony cache seed has no matching successful cook')
        seed = previous / 'out/ddc'
    if not seed.is_dir():
        raise ValueError('Missing retained cook cache')
    subprocess.run(['cp', '-a', '--reflink=auto', str(seed), str(out / 'ddc')], check=True)
    source_script = CSS / 'tools/shared-assets/cook-shared-inner.sh'
    protected[str(source_script)] = sha(source_script)
    script = output / 'cook.sh'
    command_text = source_script.read_text()
    if args.single_thread:
        if command_text.count('-numcores=6') != 1:
            raise ValueError('Cook command layout changed')
        command_text = command_text.replace('-numcores=6',
            '-numcores=6 -asyncskinnedassetcompilation=0 -nothreading')
    script.write_text(command_text)
    protected[str(script)] = sha(script)
    protected[str(Path(__file__).resolve())] = sha(Path(__file__))
    command = ['bash', str(wrapper), 'run', '--rm', '--init', '--user', '0:0',
        '--name', output.name, '--network', 'none', '--shm-size', '1g',
        '--mount', f'type=bind,src={engine},dst=/ue,readonly',
        '--mount', f'type=bind,src={project},dst=/project',
        '--mount', f'type=bind,src={out},dst=/out',
        '--mount', f'type=bind,src={script},dst=/cook.sh,readonly', image, 'bash', '/cook.sh']
    (output / 'plan.json').write_text(json.dumps(dict(groups=groups, sources=sources,
        scope='Private cook and readback. Ship only cook_outfit/cook_shared after validation; '
              'reuse accepted dependencies, exclude editor-only inputs. Not fit or gameplay acceptance.'), indent=2) + '\n')
    (output / 'inputs.json').write_text(json.dumps(dict(files=protected, image=image, engine=version,
        command=command, cache_seed=str(seed), single_thread=args.single_thread), indent=2) + '\n')
    started = time.monotonic()
    with (output / 'console.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    changed = [path for path, digest in protected.items() if sha(Path(path)) != digest]
    (output / 'exit.json').write_text(json.dumps(dict(returncode=result.returncode,
        seconds=time.monotonic() - started, sources_unchanged=not changed, changed=changed), indent=2) + '\n')
    result.check_returncode()
    if changed:
        raise ValueError('Protected inputs changed: ' + repr(changed))
    print('Windows cook finished. Cooked imports, shader payloads and deformation still need checking.')


if __name__ == '__main__':
    main()

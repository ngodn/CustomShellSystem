"""Package the verified private Peony cook and inspect its shipping assets."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


CSS = Path(__file__).resolve().parents[2]
EVE = CSS.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dotnet', type=Path, required=True)
    parser.add_argument('--mappings', type=Path, required=True)
    parser.add_argument('--stage', choices=('peony-windows3', 'peony-windows4', 'peony-windows5'), required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(CSS / 'tools'))
    from css import GAME
    from css_convert import Converter, DEFAULT_REPAK

    cook = EVE / 'work' / args.stage
    output = cook / 'readback1'
    result = read(cook / 'exit.json')
    if result['returncode'] or not result['sources_unchanged'] or result['changed']:
        raise ValueError('Cook failed or changed its inputs')
    for name, expected in read(cook / 'inputs.json')['files'].items():
        if sha(Path(name)) != expected:
            raise ValueError('Cook input changed: ' + name)
    plan = read(cook / 'plan.json')
    groups = plan['groups']
    if len(groups['cook_outfit']) != 55 or len(groups['cook_shared']) != 10:
        raise ValueError('Unexpected shipping ownership')
    packages = sorted(groups['cook_outfit'] | groups['cook_shared'])
    if len(packages) != 65:
        raise ValueError('Overlapping shipping ownership')
    output.mkdir(exist_ok=False)
    recipe = output / Path(__file__).name
    shutil.copy2(__file__, recipe)
    source = cook / 'project/Saved/Cooked/Windows/CSSShared/Content'
    stage = output / 'stage'
    payloads, omitted = {}, []
    for package in packages:
        header = source / (package.removeprefix('/Game/') + '.uasset')
        if not header.exists():
            if package != '/Game/CSS/eins0fx/Shared/Materials/MF_TrimFlow':
                raise ValueError('Missing cooked package: ' + package)
            omitted.append(package)
            continue
        for path in sorted(header.parent.glob(header.stem + '.*')):
            if path.suffix not in ('.uasset', '.uexp', '.ubulk', '.uptnl'):
                raise ValueError('Unexpected cooked payload: ' + str(path))
            target = stage / 'MortalShell2/Content' / path.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            payloads[str(target.relative_to(stage))] = sha(path)
    packages = [p for p in packages if p not in omitted]
    converter = Converter(CSS / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    toc = output / 'CSS_PeonyCookReadback_P.utoc'
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    mount = output / 'containers'
    converter.base_containers(GAME, mount)
    dependencies = {}
    for directory, containers in (
        (CSS / 'work/three-outfit-rig-physics/eve-final-container1', None),
        (CSS / 'work/three-outfit-rig-physics/shared-cloth-native2', 'CSS_SharedAssets_P'),
    ):
        receipt = read(directory / 'verification.json')
        if not receipt['passed']:
            raise ValueError('Unverified dependency: ' + str(directory))
        directory = directory / containers if containers else directory
        for name, expected in receipt['files'].items():
            path = directory / name
            if sha(path) != expected:
                raise ValueError('Changed dependency: ' + str(path))
            dependencies[str(path)] = expected
            (mount / path.name).symlink_to(path)
    for suffix in ('.pak', '.utoc', '.ucas'):
        path = toc.with_suffix(suffix)
        (mount / path.name).symlink_to(path)
    filters = [value for package in packages for value in ('-f', '/Content/' + package[6:])]
    converter.run(converter.retoc, 'to-legacy', mount, output / 'roundtrip', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', *filters)
    for relative, expected in payloads.items():
        if Path(relative).suffix != '.uasset' and sha(output / 'roundtrip' / relative) != expected:
            raise ValueError('IoStore payload mismatch: ' + relative)
    (output / 'packages.txt').write_text('\n'.join(packages) + '\n')
    (output / 'references.txt').write_text('\n'.join(p + '.' + p.rsplit('/', 1)[1] for p in packages) + '\n')
    meshes = ['/Game/CSS/eins0fx/EveSB/Peony/Meshes/SK_Peony',
              '/Game/CSS/eins0fx/EveSB/Faces/Peony/SK_Facial1']
    (output / 'meshes.txt').write_text('\n'.join(meshes) + '\n')
    commands = {
        'decode': [str(args.dotnet), str(CSS / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'),
                   str(mount), str(args.mappings), str(output / 'packages.txt'), str(output / 'decoded.json'), '--shader-maps'],
        'meshes': [str(args.dotnet), str(CSS / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'),
                   str(mount), str(args.mappings), str(output / 'meshes.txt'), str(output / 'meshes.json'), '--mesh-digests'],
        'imports': [str(args.dotnet), str(CSS / 'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll'),
                    str(mount), str(args.mappings), str(output / 'packages.txt'), str(output / 'references.txt'),
                    str(output / 'import-check.json')],
    }
    for name, command in commands.items():
        (output / (name + '-command.json')).write_text(json.dumps(command, indent=2) + '\n')
        with (output / (name + '.log')).open('w') as log:
            process = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        (output / (name + '-exit.json')).write_text(json.dumps(dict(returncode=process.returncode)) + '\n')
        process.check_returncode()
    if set(read(output / 'decoded.json')) != set(packages) or not read(output / 'import-check.json')['passed']:
        raise ValueError('Package exports or imports did not resolve')
    (output / 'receipt.json').write_text(json.dumps(dict(
        passed=True, packages=packages, omitted_editor_function=omitted, payloads=payloads,
        dependencies=dependencies, files={str(p): sha(p) for p in (
            output / 'decoded.json', output / 'meshes.json', output / 'import-check.json',
            recipe, cook / 'plan.json', cook / 'exit.json')},
        scope='IoStore roundtrip, decoded exports and dependency closure. '
              'Deformation values, shaders, fit and gameplay require separate validation.',
        installed=False), indent=2) + '\n')
    print('Private IoStore package and dependency readback passed.')


if __name__ == '__main__':
    main()

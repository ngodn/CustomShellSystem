"""Assemble an outfit migration from verified materials and cooked rig assets."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info
from verify_outfit_material_trial import normalize


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('editor', 'cook', 'materials', 'shared', 'baseline', 'dotnet', 'mappings', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / 'work/three-outfit-rig-physics'):
        raise ValueError('Use a new private rig workspace')
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError('Protected input changed: ' + str(path))
        protected[str(path)] = digest

    for path in (args.editor / 'reload.json', args.editor / 'source-validation.json',
                 args.materials / 'verification.json', args.shared / 'verification.json'):
        if not read(path)['passed']:
            raise ValueError('Failed prerequisite: ' + str(path))
        protect(path)
    for path in (args.editor / 'reload-exit.json', args.cook / 'exit.json'):
        if read(path)['returncode'] != 0:
            raise ValueError('Failed prerequisite: ' + str(path))
        protect(path)
    plan = read(args.editor / 'input.json')
    migration = read(args.editor / 'migration.json')
    mapping = {r['old_package']: r['new_package'] for r in plan['assets']}
    shared_check, material_check = read(args.shared / 'verification.json'), read(args.materials / 'verification.json')
    for relative, digest in shared_check['payloads'].items():
        protect(args.shared / 'stage' / relative, digest)
    shared_packages = {asset_info((args.shared / 'stage' / p).read_bytes())['package']
                       for p in shared_check['payloads'] if p.endswith('.uasset')}
    expected = set(mapping.values()) - shared_packages
    if len(expected) + len(set(mapping.values()) & shared_packages) != len(mapping):
        raise ValueError('Overlapping destination mapping')
    for row in plan['assets']:
        protect(Path(row['source_file']), row['source_sha256'])
    for row in migration['assets']:
        protect(args.editor / 'Content' / (row['package'].removeprefix('/Game/') + '.uasset'), row['sha256'])
    for path in (args.editor / 'input.json', args.editor / 'migration.json', args.baseline,
                 args.cook / 'packages.txt'):
        protect(path)
    output.mkdir(parents=True)
    stage = output / 'stage'

    def copy_payload(source, relative, expected_hash=None):
        protect(source, expected_hash)
        target = stage / relative
        if target.exists():
            raise ValueError('Duplicate asset provider: ' + str(relative))
        target.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)

    for relative, digest in material_check['payloads'].items():
        copy_payload(args.materials / 'stage' / relative, relative, digest)
    cooked_packages = (args.cook / 'packages.txt').read_text().splitlines()
    if len(cooked_packages) != len(set(cooked_packages)) or not set(cooked_packages) <= set(mapping.values()):
        raise ValueError('Invalid cooked package list')
    for package in cooked_packages:
        relative = Path(package.removeprefix('/Game/')).with_suffix('.uasset')
        source = args.cook / 'cooked/CSSAuthoring/Content' / relative
        if asset_info(source.read_bytes())['package'] != package:
            raise ValueError('Cooked package identity differs')
        for suffix in ('.uasset', '.uexp', '.ubulk'):
            file = source.with_suffix(suffix)
            if suffix == '.ubulk' and not file.exists():
                continue
            protect(file)
            if package not in shared_packages:
                copy_payload(file, Path('MortalShell2/Content') / relative.with_suffix(suffix))
    headers = {asset_info(p.read_bytes())['package'] for p in stage.rglob('*.uasset')}
    if headers != expected:
        raise ValueError('Outfit inventory differs: ' + str(sorted(headers ^ expected)))
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    toc = output / 'CSS_OutfitRigNamespaceTrial_P.utoc'
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    mount, original_mount = output / 'containers', output / 'original-containers'
    converter.base_containers(GAME, mount)
    converter.base_containers(GAME, original_mount)
    for name, digest in shared_check['files'].items():
        file = args.shared / 'CSS_SharedAssets_P' / name
        protect(file, digest)
        (mount / name).symlink_to(file.resolve())
    for suffix in ('.pak', '.utoc', '.ucas'):
        file = toc.with_suffix(suffix)
        (mount / file.name).symlink_to(file)
    baseline = read(args.baseline)
    for record in baseline['packages']:
        if record['container'] not in {'CSS_UnholyGenessa_eins0fx_P', 'CSS_EveStellarBlade_eins0fx_P',
                                       'CSS_LocomotionSharedAssets_P'}:
            continue
        for row in record['files'].values():
            file = Path(row['path'])
            protect(file, row['sha256'])
            (original_mount / file.name).symlink_to(file)
    converter.run(converter.retoc, 'to-legacy', mount, output / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/UnholyGenessa/')
    payloads = {str(p.relative_to(stage)): sha(p) for p in stage.rglob('*') if p.is_file()}
    for relative, digest in payloads.items():
        if Path(relative).suffix != '.uasset' and sha(output / 'readback' / relative) != digest:
            raise ValueError('Payload changed through IoStore: ' + relative)
    (output / 'packages.txt').write_text('\n'.join(sorted(mapping.values())) + '\n')
    (output / 'original-packages.txt').write_text('\n'.join(sorted(mapping)) + '\n')
    classes = {row['package']: row['snapshot']['class'] for row in migration['assets']}
    (output / 'references.txt').write_text('\n'.join(p + '.' + p.rsplit('/', 1)[1] +
                                                 ('_C' if classes[p] in {'AnimBlueprint', 'ControlRigBlueprint'} else '')
                                                 for p in sorted(mapping.values())) + '\n')
    commands = {
        'original': [str(args.dotnet), str(ROOT / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'),
                     str(original_mount), str(args.mappings), str(output / 'original-packages.txt'),
                     str(output / 'original-decoded.json'), '--shader-maps'],
        'candidate': [str(args.dotnet), str(ROOT / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'),
                      str(mount), str(args.mappings), str(output / 'packages.txt'),
                      str(output / 'decoded.json'), '--shader-maps'],
        'imports': [str(args.dotnet), str(ROOT / 'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll'),
                    str(mount), str(args.mappings), str(output / 'packages.txt'), str(output / 'references.txt'),
                    str(output / 'import-check.json')],
    }
    for name, command in commands.items():
        (output / (name + '-command.json')).write_text(json.dumps(command, indent=2) + '\n')
        with (output / (name + '.log')).open('x') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        (output / (name + '-exit.json')).write_text(json.dumps(dict(returncode=result.returncode)) + '\n')
        if name != 'imports' or result.returncode not in (0, 1) or not (output / 'import-check.json').is_file():
            result.check_returncode()
    original, decoded = read(output / 'original-decoded.json'), read(output / 'decoded.json')
    reverse = {v: k for k, v in mapping.items()}
    differences = [old for old, new in mapping.items() if original[old] != normalize(decoded[new], reverse)]
    comparison = dict(passed=not differences, compared=len(mapping), differences=differences)
    (output / 'comparison.json').write_text(json.dumps(comparison, indent=2) + '\n')
    for path, digest in protected.items():
        if sha(Path(path)) != digest:
            raise ValueError('Protected input changed: ' + path)
    report = dict(passed=not differences and read(output / 'import-check.json')['passed'],
                  packages=len(expected), compared=len(mapping), payloads=payloads, protected=protected,
                  files={toc.with_suffix(s).name: sha(toc.with_suffix(s)) for s in ('.pak', '.utoc', '.ucas')},
                  installed=False, scope='Decoded exported fields and payload roundtrip, with only the new shared '
                  'pack and outfit mounted. Mesh geometry and gameplay require separate verification.')
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    if not report['passed']:
        raise ValueError('Cooked comparison or dependency check failed; inspect retained reports')
    print(json.dumps(dict(packages=len(expected), compared=len(mapping), passed=True)))


if __name__ == '__main__':
    main()

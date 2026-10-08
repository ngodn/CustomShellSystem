"""Assemble verified shared payloads under one container, without installing it."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--animations', type=Path, required=True)
    parser.add_argument('--astral', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT/'work'):
        raise ValueError('Use a new private output directory')
    plan = read(args.plan)
    expected = {r['new_package'] for r in plan['assets'] if r['target_container'] == 'CSS_SharedAssets_P'}
    animation = read(args.animations/'verification.json')
    astral = read(args.astral/'pack/roundtrip.json')
    checks = [args.animations/'verification.json', args.astral/'pack/roundtrip.json',
              args.astral/'material-check.json', args.astral/'pose-check.json', args.astral/'comparison.json']
    if any(read(p)['passed'] is not True for p in checks):
        raise ValueError('Source verification is incomplete')
    payloads = {}
    for path, checksum in animation['payloads'].items():
        source = args.animations/'stage'/path
        if sha(source) != checksum:
            raise ValueError('Animation payload changed: '+str(source))
        payloads[path] = source
    for row in astral['exports']:
        source = args.astral/'pack/legacy'/row['path']
        if sha(source) != row['sha256']:
            raise ValueError('Astral payload changed: '+str(source))
    for source in (args.astral/'pack/legacy').rglob('*'):
        if source.is_file():
            relative = str(source.relative_to(args.astral/'pack/legacy'))
            if relative in payloads:
                raise ValueError('Shared package providers overlap: '+relative)
            payloads[relative] = source
    selected, headers = {}, set()
    for relative, source in payloads.items():
        if source.suffix not in {'.uasset', '.uexp', '.ubulk'}:
            raise ValueError('Unexpected payload: '+relative)
        package = '/Game/'+str(Path(relative).relative_to('MortalShell2/Content').with_suffix(''))
        if package not in expected:
            continue
        if source.suffix == '.uasset':
            if asset_info(source.read_bytes())['package'] != package:
                raise ValueError('Package header does not match the migration plan: '+package)
            headers.add(package)
        selected[relative] = source
    if headers != expected:
        raise ValueError('Shared package inventory differs: '+str(sorted(headers ^ expected)))
    output.mkdir(parents=True)
    stage = output/'stage'
    hashes = {}
    for relative, source in selected.items():
        target = stage/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
        hashes[relative] = sha(source)
        if sha(target) != hashes[relative]:
            raise ValueError('Staged payload changed: '+relative)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    container = output/'CSS_SharedAssets_P'
    container.mkdir()
    toc = container/'CSS_SharedAssets_P.utoc'
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    mount = output/'containers'
    converter.base_containers(GAME, mount)
    for path, checksum in animation['dependencies'].items():
        source = Path(path)
        if sha(source) != checksum or source.name.startswith('CSS_SharedAssets_P.'):
            raise ValueError('Invalid baseline dependency: '+path)
        (mount/source.name).symlink_to(source)
    for file in container.iterdir():
        (mount/file.name).symlink_to(file)
    converter.run(converter.retoc, 'to-legacy', mount, output/'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/Shared/')
    for relative, checksum in hashes.items():
        if Path(relative).suffix != '.uasset' and sha(output/'readback'/relative) != checksum:
            raise ValueError('Shared payload changed in roundtrip: '+relative)
    (output/'packages.txt').write_text('\n'.join(sorted(expected))+'\n')
    (output/'verification.json').write_text(json.dumps(dict(passed=True, packages=len(expected),
        payloads=hashes, dependencies=animation['dependencies'],
        evidence={str(p.resolve()): sha(p) for p in [args.plan, *checks]},
        files={p.name: sha(p) for p in container.iterdir()}, installed=False,
        scope='Shared asset payload assembly only. Runtime/catalog migration and game acceptance remain.'), indent=2)+'\n')
    print(f'Verified {len(expected)} shared packages in CSS_SharedAssets_P; not installed')


if __name__ == '__main__':
    main()

"""Roundtrip a cooked namespace batch, pinning its original decode dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--cook', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--dependency', action='append', default=[])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    migration = read(args.editor/'migration.json')
    if (not read(args.editor/'reload.json')['passed'] or
            not read(args.editor/'source-validation.json')['passed'] or
            read(args.editor/'reload-exit.json')['returncode'] != 0 or
            read(args.cook/'exit.json')['returncode'] != 0):
        raise ValueError('Editor reload and cook must pass')
    packages = (args.cook/'packages.txt').read_text().splitlines()
    if packages != [row['package'] for row in migration['assets']]:
        raise ValueError('Cook list differs from migration')
    protected = {}

    def protect(path, expected=None):
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError('Changed input: '+str(path))
        protected[str(path.resolve())] = digest

    for name in ['migration.json', 'reload.json', 'source-validation.json']:
        protect(args.editor/name)
    plan = read(args.editor/'input.json')
    if 'native_reference' in plan:
        native = plan['native_reference']
        protect(Path(native['receipt']), native['receipt_sha256'])
        protect(Path(native['source']), native['sha256'])
        protect(args.editor/'Content'/(native['package'].removeprefix('/Game/')+'.uasset'), native['sha256'])
    protect(args.baseline)
    protect(args.cook/'exit.json')
    protect(args.cook/'packages.txt')
    for row in migration['assets']:
        protect(args.editor/'Content'/(row['package'].removeprefix('/Game/')+'.uasset'), row['sha256'])
    args.output.mkdir(parents=True)
    stage = args.output/'stage'
    payloads = {}
    for package in packages:
        relative = Path(package.removeprefix('/Game/'))
        header = args.cook/'cooked/CSSAuthoring/Content'/relative.with_suffix('.uasset')
        if asset_info(header.read_bytes())['package'] != package:
            raise ValueError('Cooked header has a different package identity')
        for suffix in ['.uasset', '.uexp', '.ubulk']:
            source = header.with_suffix(suffix)
            if suffix == '.ubulk' and not source.exists():
                continue
            target = stage/'MortalShell2/Content'/relative.with_suffix(suffix)
            target.parent.mkdir(parents=True, exist_ok=True)
            protect(source)
            subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
            payloads[str(target.relative_to(stage))] = sha(source)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, args.output)
    candidate = args.output/'CSS_NamespaceAnimationTrial_P'
    candidate.mkdir()
    toc = candidate/'CSS_NamespaceAnimationTrial_P.utoc'
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    mount = args.output/'containers'
    converter.base_containers(GAME, mount)
    dependencies = {}
    baseline = read(args.baseline)
    if len(set(args.dependency)) != len(args.dependency):
        raise ValueError('Duplicate dependency')
    for name in args.dependency:
        records = [p for p in baseline['packages'] if p['container'] == name]
        if len(records) != 1:
            raise ValueError('Unknown baseline dependency: '+name)
        for record in records[0]['files'].values():
            path = Path(record['path'])
            protect(path, record['sha256'])
            (mount/path.name).symlink_to(path)
            dependencies[str(path)] = record['sha256']
    for file in candidate.iterdir():
        (mount/file.name).symlink_to(file.resolve())
    converter.run(converter.retoc, 'to-legacy', mount, args.output/'readback',
                  '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/')
    for relative, digest in payloads.items():
        path = args.output/'readback'/relative
        if path.suffix == '.uasset':
            asset_info(path.read_bytes())
        elif sha(path) != digest:
            raise ValueError('Export/bulk payload changed in roundtrip: '+relative)
    for name, digest in protected.items():
        if sha(Path(name)) != digest:
            raise ValueError('Protected input changed: '+name)
    result = dict(passed=True, packages=len(packages), payloads=payloads, dependencies=dependencies,
                  protected=protected, files={p.name: sha(p) for p in candidate.iterdir()}, installed=False,
                  scope='Container and payload roundtrip. Original dependencies are explicitly mounted; complete consolidation and gameplay acceptance are not claimed.')
    (args.output/'verification.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(packages=len(packages), payload_roundtrip=True, installed=False)))


if __name__ == '__main__':
    main()

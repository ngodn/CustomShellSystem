"""Merge verified native alternatives into a private consolidated shared pack."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info
from css_facial_resources import encode, externalize, resolve


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('shared', 'native', 'catalog', 'baseline', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT/'work'):
        raise ValueError('Use a new private output')
    inputs, payloads, sources, packages = {}, {}, {}, set()

    def protect(path, expected=None):
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError('Changed input: '+str(path))
        inputs[str(path.resolve())] = digest
        return digest

    catalog_check = read(args.catalog/'verification.json')
    if not catalog_check['passed'] or catalog_check['added_native'] != 59:
        raise ValueError('Native catalog is not verified')
    protect(args.catalog/'verification.json')
    for name, digest in catalog_check['inputs'].items():
        protect(Path(name), digest)
    for directory in (args.shared, args.native):
        receipt = read(directory/'verification.json')
        if not receipt['passed']:
            raise ValueError('Unverified asset source')
        protect(directory/'verification.json')
        for relative, digest in receipt['payloads'].items():
            path = Path(relative)
            if path.is_absolute() or '..' in path.parts or path.suffix not in ('.uasset','.uexp','.ubulk'):
                raise ValueError('Invalid payload path')
            source = directory/'stage'/path
            protect(source, digest)
            if relative in payloads and payloads[relative] != digest:
                raise ValueError('Conflicting asset providers: '+relative)
            if source.suffix == '.uasset':
                package = asset_info(source.read_bytes())['package']
                if not package.startswith('/Game/CSS/eins0fx/Shared/'):
                    raise ValueError('Unexpected shared asset owner: '+package)
                packages.add(package)
            payloads[relative], sources[relative] = digest, source
    if len(packages) != read(args.shared/'verification.json')['packages'] + 66:
        raise ValueError('Expected 59 sequences and seven carriers added; codec must be identical')
    protect(args.baseline)
    protect(args.catalog/'animation-library.json')
    baseline = read(args.baseline)
    candidate = copy.deepcopy(baseline)
    library = read(args.catalog/'animation-library.json')
    previous = baseline['catalog']['animation_library']
    if {k:v for k,v in previous.items() if k != 'entries'} != {k:v for k,v in library.items() if k != 'entries'}:
        raise ValueError('Existing adapters or facial data changed')
    if len(previous['entries']) != len(library['entries']):
        raise ValueError('Entry inventory changed')
    human = '/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton'
    if len(library['entries']) != 63 or any(
            sum(a['skeleton'] == human for a in entry['assets']) != 1 for entry in library['entries']):
        raise ValueError('Expected exactly one native alternative for each of the 63 entries')
    for old, new in zip(previous['entries'], library['entries'], strict=True):
        if (old['id'] != new['id'] or
                {k:v for k,v in old.items() if k != 'assets'} != {k:v for k,v in new.items() if k != 'assets'} or
                old['assets'] != new['assets'][:len(old['assets'])]):
            raise ValueError('Existing animation choices changed')
        for asset in new['assets'][len(old['assets']):]:
            for field in ('asset', 'idle_blend_space'):
                if field in asset and asset[field].split('.')[0] not in packages:
                    raise ValueError('Native catalog reference missing from merged pack')
    candidate['catalog']['animation_library'] = library
    output.mkdir(parents=True)
    stage = output/'stage'
    for relative, source in sources.items():
        target = stage/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.hardlink_to(source)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    container = output/'CSS_SharedAssets_P'
    container.mkdir()
    toc = container/'CSS_SharedAssets_P.utoc'
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    for suffix in ('.utoc','.ucas'):
        path = container/('CSS_SharedAssets_P'+suffix)
        candidate['containers'][suffix] = dict(file=path.name, bytes=path.stat().st_size, sha256=sha(path))
    manifest, resources = externalize(candidate)
    metadata = output/'metadata'
    folder = metadata/'MortalShell2/Content/CSS/Packages'/candidate['id']
    folder.mkdir(parents=True)
    (folder/'manifest.json').write_bytes(encode(manifest))
    for name, payload in resources.items():
        (folder/name).write_bytes(payload)
    pak = container/'CSS_SharedAssets_P.pak'
    converter.run(DEFAULT_REPAK, 'pack', metadata, pak, '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', pak, '--output', output/'metadata-readback')
    unpacked = output/'metadata-readback'/folder.relative_to(metadata)
    if resolve(read(unpacked/'manifest.json'), {name:(unpacked/name).read_bytes() for name in resources}) != candidate:
        raise ValueError('Packed catalog readback differs')
    mount = output/'containers'
    converter.base_containers(GAME, mount)
    for path in (toc, container/'CSS_SharedAssets_P.ucas'):
        (mount/path.name).symlink_to(path)
    converter.run(converter.retoc, 'to-legacy', mount, output/'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/Shared/')
    for relative, digest in payloads.items():
        path = output/'readback'/relative
        if path.suffix == '.uasset':
            if asset_info(path.read_bytes())['package'] != asset_info(sources[relative].read_bytes())['package']:
                raise ValueError('Readback package identity changed')
        elif sha(path) != digest:
            raise ValueError('Readback payload changed: '+relative)
    for name, digest in inputs.items():
        if sha(Path(name)) != digest:
            raise ValueError('Input changed during merge: '+name)
    (output/'shared-inline.json').write_bytes(encode(candidate))
    (output/'packages.txt').write_text('\n'.join(sorted(packages))+'\n')
    (output/'verification.json').write_text(json.dumps(dict(passed=True, installed=False,
        packages=len(packages), native_entries=63, added_packages=66, inputs=inputs,
        payloads=payloads, files={p.name:sha(p) for p in container.iterdir()},
        scope='Shared asset and catalog round trip. Original payloads and custom-rig/facial definitions preserved. Native gameplay and visual acceptance remain.'),indent=2)+'\n')
    print('Merged',len(packages),'shared packages; preserved existing assets and catalog fields')


if __name__ == '__main__':
    main()

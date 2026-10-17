"""Bundle the verified shoulder rigs into a private Shared Assets candidate.

Python 3.14. Does not install files or change the released archive.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME, sha
from css_convert import Converter, DEFAULT_REPAK

NAME = 'CSS_SharedAssets_P'
RIGS = {'CR_EveShoulderSupport', 'CR_UnholyShoulderSupport', 'CR_CommanderBodyShoulderSupport'}


def read(path):
    return json.loads(path.read_text())


def require(value, message):
    if not value:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-zip', type=Path, required=True)
    parser.add_argument('--rig-pack', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', help='Stamp both catalog version fields for a coordinated update')
    args = parser.parse_args()
    require(args.version is None or re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', args.version),
            'Expected a stable semantic version')
    out = args.output.absolute()
    require(not out.exists() and out.resolve().is_relative_to((ROOT / 'work').resolve()),
            'Use a new private directory beneath work')
    baseline, rigs = args.baseline_zip.resolve(strict=True), args.rig_pack.resolve(strict=True)
    proof = read(rigs / 'receipt.json')
    require(proof['passed'] and proof['exports_roundtrip'], 'Rig package is unverified')
    pins = {str(baseline): sha(baseline), str(rigs / 'receipt.json'): sha(rigs / 'receipt.json')}
    for name, row in proof['files'].items():
        require(sha(rigs / name) == row['sha256'], 'Rig container changed')
        pins[str(rigs / name)] = row['sha256']
    out.mkdir(parents=True)
    original = out / 'baseline'
    with zipfile.ZipFile(baseline) as archive:
        expected = {f'{NAME}/{NAME}{suffix}' for suffix in ('.pak', '.utoc', '.ucas')}
        require(set(archive.namelist()) == expected, 'Unexpected baseline archive layout')
        archive.extractall(original)
    base = original / NAME
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    mount = out / 'containers'
    converter.base_containers(GAME, mount)
    for source in (base, rigs):
        for suffix in ('.utoc', '.ucas'):
            path, = source.glob('*' + suffix)
            (mount / path.name).symlink_to(path)
    stage = out / 'stage'
    converter.run(converter.retoc, 'to-legacy', mount, stage, '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '--no-script-objects', '-f', '/Content/CSS/')
    listing = converter.run(converter.retoc, 'list', base / (NAME + '.utoc'), '--path')
    require('ShaderCodeLibrary' not in listing, 'Shader containers require shader-preserving assembly')
    baseline_paths = set(re.findall(r'ExportBundleData\s+\.\./\.\./\.\./(.+)', listing))
    additions = {f'MortalShell2/Content/CSS/Runtime/Hands/{name}.uasset' for name in RIGS}
    packages = {str(p.relative_to(stage)) for p in stage.rglob('*.uasset')}
    require(packages == baseline_paths | additions and not baseline_paths & additions,
            'Unexpected, missing or duplicate package')
    payloads = {str(p.relative_to(stage)): sha(p) for p in stage.rglob('*') if p.is_file()}
    pack = out / 'pack' / NAME
    pack.mkdir(parents=True)
    toc = pack / (NAME + '.utoc')
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    metadata = out / 'metadata'
    converter.run(DEFAULT_REPAK, 'unpack', base / (NAME + '.pak'), '--output', metadata)
    manifest_path, = metadata.rglob('manifest.json')
    manifest = read(manifest_path)
    previous = copy.deepcopy(manifest)
    if args.version:
        for document in (manifest, previous):
            document['version'] = args.version
            document['catalog']['animation_library']['version'] = args.version
    for suffix in ('.utoc', '.ucas'):
        path = toc.with_suffix(suffix)
        manifest['containers'][suffix] = dict(file=path.name, bytes=path.stat().st_size, sha256=sha(path))
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    require({k:v for k,v in previous.items() if k != 'containers'} ==
            {k:v for k,v in manifest.items() if k != 'containers'}, 'Catalog changed')
    converter.run(DEFAULT_REPAK, 'pack', metadata, toc.with_suffix('.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', toc.with_suffix('.pak'), '--output', out / 'metadata-readback')
    for path in metadata.rglob('*'):
        if path.is_file():
            require(sha(path) == sha(out / 'metadata-readback' / path.relative_to(metadata)),
                    'Metadata roundtrip changed')
    for path in list(mount.glob('CSS_*')):
        path.unlink()
    for path in pack.iterdir():
        (mount / path.name).symlink_to(path)
    converter.run(converter.retoc, 'to-legacy', mount, out / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '--no-script-objects', '-f', '/Content/CSS/')
    for relative, digest in payloads.items():
        require(sha(out / 'readback' / relative) == digest, 'Payload changed: ' + relative)
    package_list = out / 'rig-packages.txt'
    package_list.write_text('\n'.join('/Game/CSS/Runtime/Hands/' + name for name in sorted(RIGS)) + '\n')
    command = [Path('/home/eins0fx/.local/share/mise/installs/dotnet/10.0.401/dotnet'),
               ROOT / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll', mount,
               GAME / 'Binaries/Win64/ue4ss/MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241+1339-d7e7826d.usmap',
               package_list, out / 'decoded.json']
    with (out / 'decode.log').open('w') as log:
        subprocess.run(list(map(str, command)), stdout=log, stderr=subprocess.STDOUT, check=True)
    require(read(out / 'decoded.json') == read(rigs / 'decoded.json'), 'Decoded rigs changed')
    require(all(sha(Path(path)) == digest for path, digest in pins.items()), 'Inputs changed')
    archive = out / f'CSS_SharedAssets_P-v{args.version or "1.0.1-candidate"}.zip'
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(pack.iterdir()):
            bundle.write(path, str(path.relative_to(pack.parent)))
    with zipfile.ZipFile(archive) as bundle:
        require(bundle.testzip() is None, 'ZIP integrity failed')
        for path in pack.iterdir():
            require(hashlib.sha256(bundle.read(f'{NAME}/{path.name}')).hexdigest() == sha(path),
                    'ZIP payload changed')
    receipt = dict(passed=True, installed=False, inputs=pins, baseline_packages=len(baseline_paths),
                   added_packages=len(additions), total_packages=len(packages),
                   exact_roundtrip_files=len(payloads), catalog_preserved=True,
                   decoded_rigs_preserved=True, files={p.name:sha(p) for p in pack.iterdir()},
                   archive=dict(path=str(archive), sha256=sha(archive)),
                   replaces='CSS_HandSupport_P must be retired when installing this shared candidate.',
                   version=args.version,
                   release_status='Prepared locally; not published.' if args.version else
                       'Private candidate; catalog versions remain at the baseline release.')
    (out / 'verified.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()

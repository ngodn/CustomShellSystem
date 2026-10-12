"""Package accepted Peony assets with version-only metadata changes. Python 3.14."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css_package import verify

EVE = 'CSS_EveStellarBlade_eins0fx_P'
SHARED = 'CSS_SharedAssets_P'
HASHES = {
    EVE: ('d0ff238fd26f692f9f4f7eadb7f8b96240cd05b26023b589c773d4b6212a79d4',
          '17307bca225d0b3e1a7678ba27c32c771c975dced11d7710e50a94e1a6e20068',
          '31eae2443d26d1b8f224db8850fe9970815e3da14186d7021d91cdb25e5b9053'),
    SHARED: ('3b8f2a7bf850ebf51ae3556d1aac248dc8f387cb5751c07345a1c7296696730e',
             'b71dcf1a66047ca6035c9b41074aa31fa0a3f2930821a663fba749a37ee677aa',
             'aecee797b0a8ed983494087c78b18267dfa7b8e30f96e9c57bf1e33de675e64d'),
}


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def run(log, *command):
    with log.open('x') as stream:
        subprocess.run([str(x) for x in command], stdout=stream,
                       stderr=subprocess.STDOUT, check=True)


def archive_trio(folder, destination):
    files = {p.name: sha(p) for p in folder.iterdir()}
    expected = {folder.name + suffix for suffix in ('.pak', '.utoc', '.ucas')}
    if set(files) != expected:
        raise ValueError('Unexpected package members')
    with zipfile.ZipFile(destination, 'x', compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6) as archive:
        for name in sorted(files):
            archive.write(folder / name, f'{folder.name}/{name}')
    with zipfile.ZipFile(destination) as archive:
        if len(archive.infolist()) != 3 or set(archive.namelist()) != {
                f'{folder.name}/{name}' for name in files}:
            raise ValueError('Unexpected ZIP members')
        for name, digest in files.items():
            with archive.open(f'{folder.name}/{name}') as stream:
                if hashlib.file_digest(stream, 'sha256').hexdigest() != digest:
                    raise ValueError('ZIP payload mismatch: ' + name)
    checksum = sha(destination)
    destination.with_suffix('.zip.sha256').write_text(f'{checksum}  {destination.name}\n')
    return {'file': destination.name, 'sha256': checksum, 'files': files}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--accepted', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dist', type=Path, required=True)
    parser.add_argument('--tools-root', type=Path, default=ROOT)
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--shared-metadata', type=Path,
                        help='Verified versioned shared pak pinned by packaging/shared-assets.json')
    args = parser.parse_args()
    revision = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
    if not args.prepare_only:
        tagged = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse',
                                         'v1.0.0-beta.12^{commit}'], text=True).strip()
        dirty = subprocess.check_output(['git', '-C', str(ROOT), 'status', '--porcelain'], text=True)
        if tagged != revision or dirty:
            raise ValueError('Build the release from the clean beta.12 tag')
        spec = read(ROOT / 'packaging/shared-assets.json')
        if args.shared_metadata is None or sha(args.shared_metadata) != spec['files'][SHARED + '.pak']:
            raise ValueError('Supply the shared metadata pak pinned by the release specification')
    source = args.accepted.resolve(strict=True)
    out = args.output.resolve()
    dist = args.dist.resolve()
    repak = args.tools_root / 'build/repak/release/repak'
    retoc = args.tools_root / 'build/retoc-css-target/release/retoc'
    if out.exists() or dist.exists():
        raise FileExistsError('Use fresh staging and delivery directories')
    acceptance = read(source / 'live-accepted.json')
    if acceptance['user_answer'] != 'Motion looks right now' or not acceptance['live_limits_verified']:
        raise ValueError('Accepted motion receipt missing')
    for name, hashes in HASHES.items():
        for suffix, digest in zip(('.pak', '.utoc', '.ucas'), hashes, strict=True):
            if sha(source / 'pack' / name / (name + suffix)) != digest:
                raise ValueError('Accepted input changed: ' + name + suffix)
    out.mkdir(parents=True)
    dist.mkdir(parents=True)
    report = {'source_commit': revision,
        'acceptance_sha256': sha(source / 'live-accepted.json'), 'packages': {}}
    for name, version in ((EVE, '1.5.0'), (SHARED, '1.0.0-beta.12')):
        metadata = out / 'metadata' / name
        metadata.mkdir(parents=True)
        run(out / (name + '-unpack.log'), repak, 'unpack',
            source / 'pack' / name / (name + '.pak'), '--output', metadata)
        manifest_path, = metadata.rglob('manifest.json')
        original = read(manifest_path)
        manifest = copy.deepcopy(original)
        manifest['version'] = version
        if name == EVE:
            outfit, = manifest['catalog']['outfits']
            if [v['id'] for v in outfit['variants']] != [
                    'black_pearl', 'prototype', 'skin_suit', 'bikini',
                    'casual_sweater', 'midsummer_alice', 'peony']:
                raise ValueError('Unexpected outfit scope')
            outfit['version'] = version
        else:
            manifest['catalog']['animation_library']['version'] = version
        manifest_path.write_text(json.dumps(manifest, separators=(',', ':')) + '\n')
        folder = out / 'pack' / name
        folder.mkdir(parents=True)
        for suffix in ('.utoc', '.ucas'):
            (folder / (name + suffix)).hardlink_to(source / 'pack' / name / (name + suffix))
        run(out / (name + '-pack.log'), repak, 'pack', metadata,
            folder / (name + '.pak'), '--version', 'V8B')
        if name == SHARED and not args.prepare_only:
            # repak's index ordering can change bytes with identical extracted metadata.
            # Use the frozen dependency artifact, then compare every decoded file below.
            shutil.copyfile(args.shared_metadata, folder / (name + '.pak'))
        readback = out / 'readback' / name
        readback.mkdir(parents=True)
        run(out / (name + '-readback.log'), repak, 'unpack',
            folder / (name + '.pak'), '--output', readback)
        expected = {str(p.relative_to(metadata)): sha(p) for p in metadata.rglob('*') if p.is_file()}
        actual = {str(p.relative_to(readback)): sha(p) for p in readback.rglob('*') if p.is_file()}
        if actual != expected:
            raise ValueError('Packed metadata changed')
        restored = read(readback / manifest_path.relative_to(metadata))
        restored['version'] = original['version']
        if name == EVE:
            restored['catalog']['outfits'][0]['version'] = original['catalog']['outfits'][0]['version']
            verify(folder, repak)
        else:
            restored['catalog']['animation_library']['version'] = original['catalog']['animation_library']['version']
        if restored != original:
            raise ValueError('Release changed more than versions')
        run(out / (name + '-container.log'), retoc, 'verify', folder / (name + '.utoc'))
        report['packages'][name] = {'version': version,
            'files': {p.name: sha(p) for p in folder.iterdir()},
            'asset_containers_identical_to_accepted': True,
            'metadata_changes': ['version', 'catalog.outfits[0].version'] if name == EVE else ['version', 'catalog.animation_library.version']}
    for name in ('CSS_CommanderWhite_eins0fx_P', 'CSS_UnholyGenessa_eins0fx_P'):
        (out / 'pack' / name).mkdir()
        for suffix in ('.pak', '.utoc', '.ucas'):
            (out / 'pack' / name / (name + suffix)).symlink_to(source / 'pack' / name / (name + suffix))
    run(out / 'catalog.log', args.tools_root / 'build/host/css_animation_library_tests',
        '--audited-peony-pack', out / 'pack', source / 'variants.json')
    if not args.prepare_only:
        for name in (EVE, SHARED):
            version = report['packages'][name]['version']
            report['packages'][name]['archive'] = archive_trio(
                out / 'pack' / name, dist / f'{name}-v{version}.zip')
        spec = read(ROOT / 'packaging/shared-assets.json')
        if spec['files'] != report['packages'][SHARED]['files']:
            raise ValueError('Shared dependency hashes differ from release specification')
        for source_name, target_name in (
                ('packaging/peony-release-README.md', 'README.md'),
                ('docs/releases/eve-v1.5.0.bbcode', 'eve-v1.5.0-changelog.bbcode'),
                ('packaging/css-beta12-changelog.bbcode', 'css-beta12-changelog.bbcode'),
                ('packaging/locomotion-trial-CREDITS.txt', 'ANIMATION-CREDITS.txt')):
            shutil.copyfile(ROOT / source_name, dist / target_name)
    report['passed'] = True
    report['installed'] = False
    report['uploaded'] = False
    write(out / 'verification.json', report)
    write(dist / 'verification.json', report)
    print(dist)


if __name__ == '__main__':
    main()

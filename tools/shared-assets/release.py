"""Package the audited Astral containers separately from CSS. Python 3.14."""
import argparse
import json
from pathlib import Path
import re
import sys
import time
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from css_release import ROOT, VERSION_PATTERN, digest, git
from css_paths import temporary_directory

CONTAINER = 'CSS_AstralSharedAssets_P'
PREFIX = CONTAINER + '/'
ASSETS = {CONTAINER + suffix for suffix in ('.pak', '.utoc', '.ucas')}


def verify(archive):
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        expected = ASSETS | {'README.txt', 'release.json'}
        if (len(names) != len(set(names)) or set(names) != {PREFIX + n for n in expected}
                or z.testzip() is not None):
            raise ValueError('Unexpected, duplicate or damaged shared ZIP members')
        manifest = json.loads(z.read(PREFIX + 'release.json'))
        if (manifest.get('container') != CONTAINER
                or not re.fullmatch(VERSION_PATTERN, manifest.get('version', ''))
                or manifest.get('css_version') != manifest['version']
                or set(manifest['files']) != expected - {'release.json'}):
            raise ValueError('Invalid shared release manifest')
        for name, checksum in manifest['files'].items():
            if digest(z.read(PREFIX + name)) != checksum:
                raise ValueError('Shared payload checksum mismatch: ' + name)
        if ('CSS v' + manifest['css_version']).encode() not in z.read(PREFIX + 'README.txt'):
            raise ValueError('Shared README version mismatch')
        return manifest


def build(pack, output):
    version = (ROOT / 'VERSION').read_text().strip()
    revision = git('rev-parse', 'HEAD')
    if not re.fullmatch(VERSION_PATTERN, version):
        raise ValueError('Invalid VERSION')
    if git('rev-parse', f'v{version}^{{commit}}') != revision or git('status', '--porcelain'):
        raise ValueError('Build shared assets from the clean version tag')
    spec = json.loads((ROOT / 'packaging/astral-shared-assets.json').read_text())
    if spec['container'] != CONTAINER or set(spec['files']) != ASSETS:
        raise ValueError('Unexpected audited container set')
    files = {name: (pack / name).read_bytes() for name in sorted(ASSETS)}
    for name, data in files.items():
        if digest(data) != spec['files'][name]:
            raise ValueError('Container differs from audited candidate: ' + name)
    for name, checksum in spec['evidence'].items():
        data = (pack / name).read_bytes()
        if digest(data) != checksum or json.loads(data).get('passed') is not True:
            raise ValueError('Shared validation receipt differs: ' + name)
    files['README.txt'] = (ROOT / 'packaging/astral-shared-README.txt').read_text().replace('@VERSION@', version).encode()
    manifest = {'version': version, 'css_version': version, 'source_commit': revision,
                'container': CONTAINER, 'asset_count': spec['asset_count'],
                'evidence': spec['evidence'],
                'files': {name: digest(data) for name, data in sorted(files.items())}}
    files['release.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f'CSS-AstralSharedAssets-v{version}.zip'
    if archive.exists():
        raise FileExistsError(archive)
    timestamp = time.gmtime(max(315532800, int(git('show', '-s', '--format=%ct', 'HEAD'))))[:6]
    with temporary_directory(output, 'shared-stage') as stage:
        candidate = stage / archive.name
        with zipfile.ZipFile(candidate, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for name, data in sorted(files.items()):
                info = zipfile.ZipInfo(PREFIX + name, timestamp)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                z.writestr(info, data)
        verify(candidate)
        candidate.rename(archive)
    archive.with_suffix('.zip.sha256').write_text(f'{digest(archive.read_bytes())}  {archive.name}\n')
    print(archive)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    release = sub.add_parser('build')
    release.add_argument('pack', type=Path)
    release.add_argument('--output', type=Path, required=True)
    check = sub.add_parser('verify')
    check.add_argument('archive', type=Path)
    args = parser.parse_args()
    if args.action == 'build':
        build(args.pack.resolve(strict=True), args.output.resolve())
    else:
        print(json.dumps(verify(args.archive), indent=2))

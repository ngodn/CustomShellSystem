#!/usr/bin/env python3
"""Build and verify the clean CSS runtime ZIP. Python 3.14, standard library only."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PREFIX = 'CustomShellSystem/'


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def git(*args: str) -> str:
    return subprocess.check_output(['git', '-C', str(ROOT), *args], text=True).strip()


def payload_names(version: str, interface: str = "inventory") -> set[str]:
    files = {'enabled.txt', 'core.json', 'dlls/main.dll', f'cores/css_core-{version}.dll',
             'README.txt', 'THIRD_PARTY_NOTICES.txt'}
    if interface == 'standalone':
        files.add('assets/wardrobe-v1.png')
    elif interface == 'inventory':
        files.add('assets/inventory-logo-v1.png')
    else:
        raise ValueError('Unknown runtime interface')
    return files


def verify(archive: Path) -> dict:
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        if len(names) != len(set(names)) or z.testzip() is not None:
            raise ValueError('Duplicate or damaged ZIP members')
        manifest = json.loads(z.read(PREFIX + 'release.json'))
        version = manifest['version']
        if not re.fullmatch(r'\d+\.\d+\.\d+', version):
            raise ValueError('Invalid release version')
        expected = payload_names(version, manifest.get("interface", "standalone"))
        if set(names) != {PREFIX + p for p in expected | {'release.json'}}:
            raise ValueError('ZIP contains unexpected or missing files')
        if set(manifest['files']) != expected:
            raise ValueError('Release manifest does not match payload allowlist')
        for path, checksum in manifest['files'].items():
            if digest(z.read(PREFIX + path)) != checksum:
                raise ValueError(f'Payload checksum mismatch: {path}')
        core = json.loads(z.read(PREFIX + 'core.json'))
        if core != {'abi': 1, 'file': f'css_core-{version}.dll'}:
            raise ValueError('Core selection does not match the bundled core')
        for path in ['dlls/main.dll', f'cores/css_core-{version}.dll']:
            data = z.read(PREFIX + path)
            if len(data) < 64 or data[:2] != b'MZ':
                raise ValueError(f'Not a Windows DLL: {path}')
            offset = struct.unpack_from('<I', data, 60)[0]
            if offset + 24 > len(data) or data[offset:offset+4] != b'PE\0\0':
                raise ValueError(f'Invalid PE header: {path}')
            if struct.unpack_from('<H', data, offset+4)[0] != 0x8664 or not struct.unpack_from('<H', data, offset+22)[0] & 0x2000:
                raise ValueError(f'Expected x64 DLL: {path}')
        if z.read(PREFIX + 'enabled.txt') != b'':
            raise ValueError('Unexpected activation marker contents')
        if f'MSII - CSS v{version}'.encode() not in z.read(PREFIX + 'README.txt'):
            raise ValueError('README version mismatch')
        return manifest


def build(args: argparse.Namespace) -> Path:
    version = (ROOT / 'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Invalid VERSION')
    revision = git('rev-parse', 'HEAD')
    if git('rev-parse', f'v{version}^{{commit}}') != revision:
        raise ValueError('Build from the version tag, not another revision')
    if git('status', '--porcelain', '--untracked-files=normal'):
        raise ValueError('Release source tree must be clean')
    build_dir = ROOT / 'build/release-windows'
    subprocess.run(['cmake', '-S', str(ROOT / 'native'), '-B', str(build_dir), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'native/toolchain-clang-cl.cmake'),
                    '-DCSS_SDK=' + str(args.sdk.resolve(strict=True)),
                    '-DCSS_INVENTORY_DEV=OFF', '-DCSS_TRANSITION_TESTS=OFF'], check=True)
    subprocess.run(['cmake', '--build', str(build_dir), '--target', 'main', 'css_core', '-j', '4'], check=True)
    # Copy only build outputs and explicit tracked runtime assets. Never read a live installation.
    files = {'enabled.txt': b'',
             'assets/inventory-logo-v1.png': (ROOT / 'assets/inventory-logo-v1.png').read_bytes(),
             'core.json': (json.dumps({'abi': 1, 'file': f'css_core-{version}.dll'}, indent=2) + '\n').encode(),
             'dlls/main.dll': (build_dir / 'main.dll').read_bytes(),
             f'cores/css_core-{version}.dll': (build_dir / 'css_core.dll').read_bytes(),
             'README.txt': (ROOT / 'packaging/README.txt').read_text().replace('@VERSION@', version).encode(),
             'THIRD_PARTY_NOTICES.txt': (ROOT / 'packaging/THIRD_PARTY_NOTICES.txt').read_bytes()}
    manifest = {'version': version, 'source_commit': revision, 'abi': 1, 'interface': 'inventory',
                'ue4ss_revision': 'd7e7826d415b0332b43439a64e6c87f64019be03',
                'files': {p: digest(data) for p, data in sorted(files.items())}}
    files['release.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f'MSII-CSS-v{version}.zip'
    if archive.exists():
        raise FileExistsError(archive)
    timestamp = time.gmtime(max(315532800, int(git('show', '-s', '--format=%ct', 'HEAD'))))[:6]
    with tempfile.TemporaryDirectory(prefix='css-release-', dir=output) as temporary:
        candidate = Path(temporary) / archive.name
        with zipfile.ZipFile(candidate, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for path, data in sorted(files.items()):
                info = zipfile.ZipInfo(PREFIX + path, timestamp)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                z.writestr(info, data)
        verify(candidate)
        candidate.rename(archive)
    checksum = digest(archive.read_bytes())
    archive.with_suffix('.zip.sha256').write_text(f'{checksum}  {archive.name}\n')
    print(archive)
    print(checksum)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    release = sub.add_parser('build')
    release.add_argument('--sdk', type=Path, default=ROOT / 'reference/ue4ss-sdk-d7e7826d')
    release.add_argument('--output', type=Path, default=ROOT / 'dist/releases')
    check = sub.add_parser('verify')
    check.add_argument('archive', type=Path)
    args = parser.parse_args()
    if args.action == 'build':
        build(args)
    else:
        print(json.dumps(verify(args.archive), indent=2))


if __name__ == '__main__':
    main()

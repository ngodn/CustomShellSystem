#!/usr/bin/env python3
"""Build the three CSSX release ZIPs from a clean, tagged source tree."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import zipfile

import css_release
import cssx_package

ROOT = Path(__file__).resolve().parents[1]


def framework_names(version: str) -> set[str]:
    return {'cssx.json', f'cores/cssx_core-{version}.dll', 'assets/cssx-logo.png',
            'CSSX-README.txt', 'CSSX-THIRD_PARTY_NOTICES.txt'}


def verify_framework(path: Path) -> dict:
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or archive.testzip():
            raise ValueError('Duplicate or damaged CSSX ZIP members')
        manifest = json.loads(archive.read('cssx-release.json'))
        version = manifest['version']
        if not isinstance(version, str) or not re.fullmatch(r'\d+\.\d+\.\d+', version):
            raise ValueError('Invalid CSSX version')
        expected = framework_names(version)
        if set(names) != expected | {'cssx-release.json'} or set(manifest['files']) != expected:
            raise ValueError('CSSX ZIP contains unexpected or missing files')
        if json.loads(archive.read('cssx.json')) != {'file': f'cssx_core-{version}.dll'}:
            raise ValueError('CSSX core selection mismatch')
        for name, digest in manifest['files'].items():
            if css_release.digest(archive.read(name)) != digest:
                raise ValueError('CSSX checksum mismatch: ' + name)
        with tempfile.TemporaryDirectory() as temporary:
            dll = Path(temporary) / 'core.dll'
            dll.write_bytes(archive.read(f'cores/cssx_core-{version}.dll'))
            cssx_package.validate_pe(dll)
        return manifest


def write_framework(output: Path, version: str, revision: str, build: Path) -> Path:
    files = {
        'cssx.json': (json.dumps({'file': f'cssx_core-{version}.dll'}, indent=2)+'\n').encode(),
        f'cores/cssx_core-{version}.dll': (build/'cssx_core.dll').read_bytes(),
        'assets/cssx-logo.png': (ROOT/'assets/cssx-logo-v4.png').read_bytes(),
        'CSSX-README.txt': (ROOT/'packaging/cssx/README.txt').read_text().replace('@VERSION@', version).encode(),
        'CSSX-THIRD_PARTY_NOTICES.txt': (ROOT/'packaging/cssx/THIRD_PARTY_NOTICES.txt').read_bytes(),
    }
    manifest = {'version': version, 'source_commit': revision, 'abi': 1,
                'files': {k: css_release.digest(v) for k, v in sorted(files.items())}}
    files['cssx-release.json'] = (json.dumps(manifest, indent=2)+'\n').encode()
    target = output / f'MSII-CSSX-v{version}.zip'
    if target.exists():
        raise FileExistsError(target)
    stamp = time.gmtime(max(315532800, int(css_release.git('show', '-s', '--format=%ct', 'HEAD'))))[:6]
    with tempfile.TemporaryDirectory(dir=output, prefix='cssx-release-') as temporary:
        candidate = Path(temporary)/target.name
        with zipfile.ZipFile(candidate, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name, data in sorted(files.items()):
                entry = zipfile.ZipInfo(name, stamp)
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, data)
        verify_framework(candidate)
        candidate.replace(target)
    return target


def build_all(output: Path, sdk: Path) -> list[Path]:
    version = (ROOT/'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Invalid VERSION')
    revision = css_release.git('rev-parse', 'HEAD')
    if css_release.git('rev-parse', f'v{version}^{{commit}}') != revision:
        raise ValueError('Build from the version tag')
    if css_release.git('status', '--porcelain', '--untracked-files=normal'):
        raise ValueError('Release source tree must be clean')
    for source in ('extensions/cheat-menu', 'examples/extensions/ui-kit'):
        if json.loads((ROOT/source/'extension.json').read_text())['version'] != version:
            raise ValueError('Extension version differs from CSS: '+source)
    output = output.resolve(); output.mkdir(parents=True, exist_ok=True)
    build = ROOT/'build/release-windows'
    subprocess.run(['cmake', '-S', str(ROOT/'native'), '-B', str(build), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release', '-DCSS_INVENTORY_DEV=OFF', '-DCSS_TRANSITION_TESTS=OFF',
                    '-DCMAKE_TOOLCHAIN_FILE='+str(ROOT/'native/toolchain-clang-cl.cmake'),
                    '-DCSS_SDK='+str(sdk.resolve(strict=True))], check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'cssx_core', 'cheat_menu', '-j4'], check=True)
    host = ROOT/'build/release-host'
    subprocess.run(['cmake', '-S', str(ROOT/'native'), '-B', str(host), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release'], check=True)
    subprocess.run(['cmake', '--build', str(host), '--target', 'cssx_validate', '-j4'], check=True)
    artifacts = [write_framework(output, version, revision, build)]
    with tempfile.TemporaryDirectory(prefix='cssx-release-extensions-') as temporary:
        for source, title in (('extensions/cheat-menu', 'CSSX-Cheat-Menu'),
                              ('examples/extensions/ui-kit', 'CSSX-UI-Kit')):
            stage = Path(temporary)/title
            # The package builder copies only declared files from this source staging tree.
            shutil.copytree(ROOT/source, stage)
            if title == 'CSSX-Cheat-Menu':
                shutil.copyfile(build/'cheat_menu.dll', stage/'cheat_menu.dll')
            name = title+'-v'+version+'.zip'
            if (output/name).exists():
                raise FileExistsError(output/name)
            artifacts.append(cssx_package.build(stage, output, host/'cssx_validate', name))
    for path in artifacts:
        path.with_suffix('.zip.sha256').write_text(css_release.digest(path.read_bytes())+'  '+path.name+'\n')
        print(path)
    return artifacts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='action', required=True)
    build = commands.add_parser('build')
    build.add_argument('--output', type=Path, default=ROOT/'dist/releases')
    build.add_argument('--sdk', type=Path, default=ROOT/'reference/ue4ss-sdk-d7e7826d')
    verify = commands.add_parser('verify')
    verify.add_argument('archive', type=Path)
    args = parser.parse_args()
    if args.action == 'build':
        build_all(args.output, args.sdk)
    else:
        print(json.dumps(verify_framework(args.archive), indent=2))


if __name__ == '__main__':
    main()

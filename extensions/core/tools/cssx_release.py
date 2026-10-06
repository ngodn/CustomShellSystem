#!/usr/bin/env python3
"""Build CSSX and extension release ZIPs from a clean tagged source tree.

  cssx_release.py build --tag v1.0.0-cssx [--output dist]
  cssx_release.py pair --tag cssx-v1.3.1 [--output dist]
  cssx_release.py verify dist/MSII-CSSX-v1.0.0.zip dist/CSSX-Cheat-Menu-v1.0.0.zip

The framework ZIP extracts to ue4ss/Mods/CSSX/. The Cheat Menu ZIP extracts
to ue4ss/Mods/CSSX/extensions/. No UE4SS binary, CSS file, state, log,
scratch file or debug tooling ships; the dev channel marker is absent so the
file request channel stays off for players.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
REPO = ROOT.parents[1]
PIN = json.loads((REPO / 'native/ue4ss-runtime.json').read_text())


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate_pe(data: bytes, name: str) -> None:
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError(f'{name}: not a Windows DLL')
    offset = int.from_bytes(data[60:64], 'little')
    pe = data[offset:offset + 24]
    if pe[:6] != b'PE\0\0\x64\x86' or not int.from_bytes(pe[22:24], 'little') & 0x2000:
        raise ValueError(f'{name}: not an x64 DLL')


def framework_files(version: str, build: Path, root: Path = ROOT) -> dict[str, bytes]:
    core = f'cssx_core-{version}.dll'
    return {
        'CSSX/enabled.txt': b'',
        'CSSX/dlls/main.dll': (build / 'main.dll').read_bytes(),
        f'CSSX/core/{core}': (build / 'cssx_core.dll').read_bytes(),
        'CSSX/core.json': (json.dumps({'file': core}, indent=2) + '\n').encode(),
        'CSSX/README.txt': (root / 'packaging/README.txt').read_text().replace('@VERSION@', version).encode(),
        'CSSX/THIRD_PARTY_NOTICES.txt': (root / 'packaging/THIRD_PARTY_NOTICES.txt').read_bytes(),
        'CSSX/assets/logo.png': (root / 'assets/logo.png').read_bytes(),
        'CSSX/assets/banner.png': (root / 'assets/banner.png').read_bytes(),
    }


def cheat_files(build: Path, repo: Path = REPO) -> tuple[str, dict[str, bytes]]:
    # The Cheat Menu carries its own version (extension.json), like Traverse; it is not
    # stamped with the CSSX framework version. Ships under extensions/.
    manifest = json.loads((repo / 'extensions/cheat-menu/extension.json').read_text())
    manifest['entry'] = 'cheat_menu.dll'
    base = 'eins0fx.cheat-menu/'
    files = {
        base + 'extension.json': (json.dumps(manifest, indent=2) + '\n').encode(),
        base + 'cheat_menu.dll': (build / 'cheat_menu.dll').read_bytes(),
        base + 'menu.json': (repo / 'extensions/cheat-menu/menu.json').read_bytes(),
        base + 'assets/banner-v1.png': (repo / 'extensions/cheat-menu/assets/banner-v1.png').read_bytes(),
    }
    for name in manifest.get('files', []):
        files[base + name] = (repo / 'extensions/cheat-menu' / name).read_bytes()
    return manifest['version'], files


def performance_files(version: str) -> dict[str, bytes]:
    manifest = json.loads((ROOT / 'performance/extension.json').read_text())
    manifest['version'] = version
    base = 'cssx.performance/'
    return {
        base + 'extension.json': (json.dumps(manifest, indent=2) + '\n').encode(),
        base + 'main.lua': (ROOT / 'performance/main.lua').read_bytes(),
        base + 'menu.json': (ROOT / 'performance/menu.json').read_bytes(),
        base + 'README.txt': (ROOT / 'performance/README.txt').read_bytes(),
    }


def traverse_files(build: Path) -> tuple[str, dict[str, bytes]]:
    # The Traverse extension carries its own version (extension.json), independent of
    # the CSSX framework version. Ships like the Cheat Menu: extracts under extensions/.
    src = REPO / 'extensions/traverse'
    manifest = json.loads((src / 'extension.json').read_text())
    manifest['entry'] = 'teleport.dll'
    base = manifest['id'] + '/'
    files = {
        base + 'extension.json': (json.dumps(manifest, indent=2) + '\n').encode(),
        base + 'teleport.dll': (build / 'teleport.dll').read_bytes(),
        base + 'menu.json': (src / 'menu.json').read_bytes(),
    }
    for name in manifest.get('files', []):
        files[base + name] = (src / name).read_bytes()
    return manifest['version'], files


def cine_files(build: Path) -> tuple[str, dict[str, bytes]]:
    # CINE carries its own version like Traverse and needs CSS 1.0.0-beta.9+ for its look track.
    src = REPO / 'extensions/cine'
    manifest = json.loads((src / 'extension.json').read_text())
    manifest['entry'] = 'cine.dll'
    base = manifest['id'] + '/'
    files = {
        base + 'extension.json': (json.dumps(manifest, indent=2) + '\n').encode(),
        base + 'cine.dll': (build / 'cine.dll').read_bytes(),
        base + 'menu.json': (src / 'menu.json').read_bytes(),
    }
    for name in manifest.get('files', []):
        files[base + name] = (src / name).read_bytes()
    # The breakdown materials: a cooked pak the player puts under Content/Paks/~mods/.
    # Paks are not kept in git. assets/pak/pack.json records the hashes of the released
    # trio; the files themselves sit beside it (build: assets/author.sh, cook.sh, pack.py).
    pak = src / 'assets/pak'
    record = json.loads((pak / 'pack.json').read_text())
    for name, expected in record['files'].items():
        if not (pak / name).is_file():
            raise FileNotFoundError(f'{pak / name} is missing; build the pak (extensions/cine/assets) and copy the trio there')
        data = (pak / name).read_bytes()
        if digest(data) != expected:
            raise ValueError(f'{name} does not match assets/pak/pack.json; rebuild the pak or update the record')
        files[f"{record['container']}/{name}"] = data
    return manifest['version'], files


def perf_tuner_files(build: Path, repo: Path = REPO) -> tuple[str, dict[str, bytes]]:
    src = repo / 'extensions/perf-tuner'
    manifest = json.loads((src / 'extension.json').read_text())
    manifest['entry'] = 'perf_tuner.dll'
    base = manifest['id'] + '/'
    files = {
        base + 'extension.json': (json.dumps(manifest, indent=2) + '\n').encode(),
        base + 'perf_tuner.dll': (build / 'perf_tuner.dll').read_bytes(),
    }
    for name in manifest.get('files', []):
        files[base + name] = (src / name).read_bytes()
    return manifest['version'], files


def write_zip(target: Path, files: dict[str, bytes], meta: dict, stamp) -> None:
    if target.exists():
        raise FileExistsError(target)
    meta = dict(meta, files={k: digest(v) for k, v in sorted(files.items())})
    files = dict(files)
    files[meta['manifest_name']] = (json.dumps(meta, indent=2) + '\n').encode()
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=target.parent, prefix='cssx-release-') as temporary:
        candidate = Path(temporary) / target.name
        with zipfile.ZipFile(candidate, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name, data in sorted(files.items()):
                entry = zipfile.ZipInfo(name, stamp)
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, data)
        verify(candidate)
        candidate.replace(target)
    (target.with_name(target.name + '.sha256')).write_text(f'{digest(target.read_bytes())}  {target.name}\n')


def verify(path: Path) -> dict:
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or archive.testzip():
            raise ValueError('Duplicate or damaged ZIP members')
        manifest_name = next((n for n in names if n.endswith('release.json')), None)
        if not manifest_name:
            raise ValueError('No release manifest')
        meta = json.loads(archive.read(manifest_name))
        if set(meta['files']) | {manifest_name} != set(names):
            raise ValueError('ZIP members differ from the manifest')
        for name, expected in meta['files'].items():
            data = archive.read(name)
            if digest(data) != expected:
                raise ValueError(f'Checksum mismatch: {name}')
            if name.endswith('.dll'):
                validate_pe(data, name)
            if len(name) > 140:
                raise ValueError(f'Member path too long: {name}')
            lower = name.lower()
            for forbidden in ('ue4ss.dll', 'css_core', '/state/', '/logs/', '/runtime/', '/dev/', '.pdb', '.log', 'request.json'):
                if forbidden in lower:
                    raise ValueError(f'Forbidden member: {name}')
        return meta


def git(*args: str, cwd: Path = REPO) -> str:
    return subprocess.run(['git', *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def export_tag(tag: str, tree: Path, repo: Path = REPO) -> str:
    commit = git('rev-parse', '--verify', tag + '^{commit}', cwd=repo)
    if git('cat-file', '-t', tag, cwd=repo) != 'tag':
        raise ValueError('Release source needs an annotated tag')
    tree.mkdir(parents=True)
    with tempfile.TemporaryFile() as archive:
        subprocess.run(['git', 'archive', '--format=tar', commit], cwd=repo, stdout=archive, check=True)
        archive.seek(0)
        with tarfile.open(fileobj=archive) as source:
            source.extractall(tree, filter='data')
    return commit


def build_pair(tag: str, output: Path) -> list[Path]:
    # A tagged archive keeps other agents' unfinished changes out without a worktree.
    with tempfile.TemporaryDirectory(prefix='cssx-pair-') as temporary:
        tree = Path(temporary) / 'src'
        commit = export_tag(tag, tree)
        core = tree / 'extensions/core'
        version = (core / 'VERSION').read_text().strip()
        if not re.fullmatch(r'\d+\.\d+\.\d+', version):
            raise ValueError('Tagged VERSION must be X.Y.Z')
        out = Path(temporary) / 'build'
        subprocess.run(['cmake', '-S', str(core), '-B', str(out), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                        '-DCMAKE_TOOLCHAIN_FILE=' + str(REPO / 'native/toolchain-clang-cl.cmake'), '-DCSSX_DEV=OFF',
                        '-DCSSX_VENDOR=' + str(REPO / 'native/vendor'), '-DCSSX_SDK=' + str(REPO / 'reference/ue4ss-sdk-d7e7826d'),
                        '-DCSSX_UE4SS_IMPORT_LIBRARY=' + str(REPO / PIN['import_library'])], check=True)
        subprocess.run(['cmake', '--build', str(out), '--target', 'main', 'cssx_core', 'cheat_menu', '-j', '8'], check=True)
        stamp = time.gmtime(max(315532800, int(git('show', '-s', '--format=%ct', commit))))[:6]
        common = {'source_commit': commit, 'tag': tag, 'extension_abi': 3, 'manifest_schema': 2, 'menu_schema': 2,
                  'loader_core_abi': 1, 'ue4ss_revision': PIN['revision'], 'ue4ss_dll_sha256': PIN['dll_sha256']}
        directory = output / f'cssx-v{version}'
        framework = directory / f'MSII-CSSX-v{version}.zip'
        cm_version, cm_files = cheat_files(out, tree)
        cheat = directory / f'CSSX-Cheat-Menu-v{cm_version}.zip'
        if framework.exists() or cheat.exists():
            raise FileExistsError('Release archives already exist; refusing to replace them')
        write_zip(framework, framework_files(version, out, core),
                  dict(common, version=version, product='CSSX', manifest_name='CSSX/release.json'), stamp)
        write_zip(cheat, cm_files, dict(common, version=cm_version, product='CSSX Cheat Menu', requires='CSSX ' + version,
                                      manifest_name='eins0fx.cheat-menu/release.json'), stamp)
        for path in [framework, cheat]:
            verify(path)
        return [framework, cheat]


def build(tag: str, output: Path, here: bool = False) -> list[Path]:
    version = (ROOT / 'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('VERSION must be X.Y.Z')
    commit = git('rev-list', '-n', '1', tag)
    if here:
        # Build in this checkout, like tools/css_release.py: it must sit exactly on the tag and be
        # clean, so the archive matches the tag without a second working tree.
        if git('rev-parse', 'HEAD') != commit:
            raise ValueError(f'--here needs the checkout on {tag}')
        if git('status', '--porcelain', '--untracked-files=normal'):
            raise ValueError('--here needs a clean checkout')
    with tempfile.TemporaryDirectory(prefix='cssx-tag-') as temporary:
        tree = REPO if here else Path(temporary) / 'src'
        if not here:
            subprocess.run(['git', 'worktree', 'add', '--detach', str(tree), commit], cwd=REPO, check=True, capture_output=True)
        try:
            core = tree / 'extensions/core'
            if (core / 'VERSION').read_text().strip() != version:
                raise ValueError('Tagged VERSION differs from the working tree')
            out = Path(temporary) / 'build'
            subprocess.run(['cmake', '-S', str(core), '-B', str(out), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                            '-DCMAKE_TOOLCHAIN_FILE=' + str(REPO / 'native/toolchain-clang-cl.cmake'), '-DCSSX_DEV=OFF',
                            '-DCSSX_VENDOR=' + str(REPO / 'native/vendor'), '-DCSSX_SDK=' + str(REPO / 'reference/ue4ss-sdk-d7e7826d'),
                            '-DCSSX_UE4SS_IMPORT_LIBRARY=' + str(REPO / PIN['import_library'])], check=True)
            subprocess.run(['cmake', '--build', str(out), '-j', '8'], check=True)
            stamp = time.gmtime(max(315532800, int(git('show', '-s', '--format=%ct', commit))))[:6]
            common = {'version': version, 'source_commit': commit, 'tag': tag, 'extension_abi': 3, 'manifest_schema': 2, 'menu_schema': 2,
                      'loader_core_abi': 1, 'ue4ss_revision': PIN['revision'], 'ue4ss_dll_sha256': PIN['dll_sha256']}
            # The CSSX framework, Cheat Menu and Performance ship together under
            # cssx-v<version>/; the Traverse extension has its own version and folder.
            fwdir = output / f'cssx-v{version}'
            fwdir.mkdir(parents=True, exist_ok=True)
            fw = fwdir / f'MSII-CSSX-v{version}.zip'
            cmvers, cmfiles = cheat_files(out)
            cm = fwdir / f'CSSX-Cheat-Menu-v{cmvers}.zip'
            write_zip(fw, framework_files(version, out), dict(common, product='CSSX', manifest_name='CSSX/release.json'), stamp)
            write_zip(cm, cmfiles, dict(common, version=cmvers, product='CSSX Cheat Menu', requires='CSSX ' + version,
                                        manifest_name='eins0fx.cheat-menu/release.json'), stamp)
            pf = fwdir / f'CSSX-Performance-v{version}.zip'
            write_zip(pf, performance_files(version), dict(common, product='CSSX Performance', requires='CSSX ' + version,
                                                        manifest_name='cssx.performance/release.json'), stamp)
            tvers, tfiles = traverse_files(out)
            tvdir = output / f'cssx-traverse-v{tvers}'
            tvdir.mkdir(parents=True, exist_ok=True)
            tv = tvdir / f'MSII-Traverse-v{tvers}.zip'
            if tv.exists():
                # Independently versioned: an unchanged Traverse keeps its published archive.
                print(f'Traverse v{tvers} is already released at {tv}; left unchanged (bump extensions/traverse/extension.json to ship a new build)')
            else:
                write_zip(tv, tfiles, dict(common, version=tvers, product='CSSX Traverse', requires='CSSX ' + version,
                                           manifest_name='eins0fx.traverse/release.json'), stamp)
            civers, cifiles = cine_files(out)
            cidir = output / f'cine-v{civers}'
            cidir.mkdir(parents=True, exist_ok=True)
            ci = cidir / f'MSII-CINE-v{civers}.zip'
            if ci.exists():
                print(f'CINE v{civers} is already released at {ci}; left unchanged (bump extensions/cine/extension.json to ship a new build)')
            else:
                write_zip(ci, cifiles, dict(common, version=civers, product='CSSX CINE', requires='CSSX ' + version,
                                           manifest_name='eins0fx.cine/release.json'), stamp)
            for name, built in (('main.dll', version), ('cssx_core.dll', version), ('cheat_menu.dll', cmvers)):
                shutil.copy2(out / name, fwdir / f'{name}.{built}.built')
            ptvers, ptfiles = perf_tuner_files(out, tree)
            ptdir = output / f'perf-tuner-v{ptvers}'
            pt = ptdir / f'MSII-Perf-Tuner-v{ptvers}.zip'
            if pt.exists():
                print(f'Perf Tuner v{ptvers} is already released at {pt}; left unchanged (bump extensions/perf-tuner/extension.json to ship a new build)')
            else:
                write_zip(pt, ptfiles, dict(common, version=ptvers, product='CSSX Perf Tuner', requires='CSSX ' + version,
                                           manifest_name='eins0fx.perf-tuner/release.json'), stamp)
            return [fw, cm, pf, tv, ci, pt]
        finally:
            if not here:
                subprocess.run(['git', 'worktree', 'remove', '--force', str(tree)], cwd=REPO, check=False, capture_output=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('build'); b.add_argument('--tag', required=True); b.add_argument('--output', type=Path, default=ROOT / 'dist')
    b.add_argument('--here', action='store_true', help='build in this checkout (clean, on the tag) instead of a temporary worktree')
    pair = sub.add_parser('pair', help='build only CSSX and Cheat Menu from a tagged archive without a worktree')
    pair.add_argument('--tag', required=True); pair.add_argument('--output', type=Path, default=ROOT / 'dist')
    v = sub.add_parser('verify'); v.add_argument('archives', nargs='+', type=Path)
    args = parser.parse_args()
    if args.cmd == 'build':
        for path in build(args.tag, args.output, args.here):
            print(path, digest(path.read_bytes()))
    elif args.cmd == 'pair':
        for path in build_pair(args.tag, args.output):
            print(path, digest(path.read_bytes()))
    else:
        for path in args.archives:
            meta = verify(path)
            print(path, 'ok', meta['product'], meta['version'], meta['source_commit'][:12])


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Build, install, reload and operate the native CSS mod. Python 3.14."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
GAME = Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2')
EXPECTED_UE4SS = '4cdd44e79df2a01fb00cf885791f933c1dd3a83324767c7a084c4da5a82f33bc'


def sha(path: Path) -> str:
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def atomic(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + '.tmp')
    with temp.open('w') as out:
        json.dump(value, out, indent=2)
        out.write('\n')
        out.flush()
        os.fsync(out.fileno())
    temp.replace(path)


def processes() -> list[int]:
    result = []
    for proc in Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            cmd = (proc / 'cmdline').read_bytes().split(b'\0')
            if cmd and cmd[0].replace(b'\\', b'/').endswith(b'/MortalShell2-Win64-Shipping.exe'):
                result.append(int(proc.name))
        except (OSError, PermissionError):
            pass
    return result


def build(core_only: bool = False) -> None:
    subprocess.run(['cmake', '-S', str(ROOT / 'native'), '-B', str(ROOT / 'build/windows'), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'native/toolchain-clang-cl.cmake')], check=True)
    subprocess.run(['cmake', '--build', str(ROOT / 'build/windows'), '-j', '4'] + (['--target', 'css_core'] if core_only else []), check=True)


def copy_verified(source: Path, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    if sha(source) != sha(target):
        raise RuntimeError(f'Copy verification failed: {target}')


def stage_core(mod: Path) -> str:
    source = ROOT / 'build/windows/css_core.dll'
    name = f'css_core-{sha(source)[:16]}-{time.time_ns()}.dll'
    copy_verified(source, mod / 'cores' / name)
    atomic(mod / 'core.json', {'abi': 1, 'file': name})
    return name


def install(game: Path) -> None:
    if processes():
        raise RuntimeError('The initial loader and asset install needs the game closed. Use reload for subsequent core updates.')
    ue4ss = game / 'Binaries/Win64/ue4ss'
    if sha(ue4ss / 'UE4SS.dll') != EXPECTED_UE4SS:
        raise RuntimeError('Installed UE4SS differs from the pinned native ABI')
    mod = ue4ss / 'Mods/CustomShellSystem'
    backup = ROOT / 'backups' / str(time.time_ns())
    backup.mkdir(parents=True)
    if mod.exists():
        shutil.copytree(mod, backup / 'CustomShellSystem')
    copy_verified(ROOT / 'build/windows/main.dll', mod / 'dlls/main.dll')
    copy_verified(ROOT / 'assets/inventory-logo-v1.png', mod / 'assets/inventory-logo-v1.png')
    (mod / 'catalog').mkdir(parents=True, exist_ok=True)
    for source in (ROOT / 'catalog').glob('*.css.json'):
        copy_verified(source, mod / 'catalog' / source.name)
    atomic(mod / 'loader-contract.json', {'abi': 1, 'dll_sha256': sha(mod / 'dlls/main.dll'),
           'sources': {name: sha(ROOT / name) for name in ('native/src/loader.cpp', 'native/src/api.hpp')}})
    name = stage_core(mod)
    (mod / 'enabled.txt').touch()
    print(f'Installed CSS loader and {name}')
    print(f'Previous CSS runtime backed up to {backup}')
    print('Install outfit trios with tools/css_package.py install PACKAGE_DIRECTORY')


def wait_json(path: Path, predicate, timeout: float = 20) -> dict:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            data = json.loads(path.read_text())
            if predicate(data):
                return data
        except (OSError, json.JSONDecodeError):
            pass
        time.sleep(0.2)
    raise TimeoutError(f'No matching live acknowledgement in {path}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=GAME)
    parser.add_argument('action', choices=['build', 'install', 'reload', 'status', 'inspect', 'enable', 'disable', 'restore', 'rescan', 'select', 'export_mappings'])
    parser.add_argument('--outfit')
    parser.add_argument('--variant')
    args = parser.parse_args()
    mod = args.game / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    if args.action == 'build':
        build()
    elif args.action == 'install':
        install(args.game)
    elif args.action == 'reload':
        if not mod.exists():
            raise RuntimeError('Install the permanent loader first')
        build(core_only=True)
        contract = json.loads((mod / 'loader-contract.json').read_text())
        if contract['abi'] != 1 or sha(mod / 'dlls/main.dll') != contract['dll_sha256']:
            raise RuntimeError('The installed loader no longer matches its ABI contract')
        if any(sha(ROOT / name) != digest for name, digest in contract['sources'].items()):
            raise RuntimeError('Loader source or ABI changed. Install that update with the game closed.')
        for source in (ROOT / 'catalog').glob('*.css.json'):
            copy_verified(source, mod / 'catalog' / source.name)
        for source in (ROOT / 'assets').glob('*.png'):
            destination = mod / 'assets' / source.name
            if not destination.exists() or sha(source) != sha(destination):
                copy_verified(source, destination)
        name = stage_core(mod)
        print(json.dumps(wait_json(mod / 'runtime/loader.json', lambda j: j.get('core') == name), indent=2))
    elif args.action == 'status':
        for name in ('loader', 'status'):
            path = mod / 'runtime' / (name + '.json')
            print(path.read_text() if path.exists() else f'{name}: no live acknowledgement yet')
    else:
        if args.action == 'select' and (not args.outfit or not args.variant):
            parser.error('select requires --outfit and --variant')
        request_id = str(time.time_ns())
        atomic(mod / 'request.json', {'id': request_id, 'action': args.action, 'outfit': args.outfit, 'variant': args.variant})
        if args.action == 'inspect':
            result = wait_json(mod / 'runtime/inspection.json', lambda j: j.get('id') == request_id)
        else:
            result = wait_json(mod / 'runtime/status.json', lambda j: j.get('last_request') == request_id)
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Build, stage and talk to standalone CSSX. Python 3.14, standard library.

  cssx.py build [--dev]                 cross-build main.dll, cssx_core.dll, cheat_menu.dll
  cssx.py stage [--dev] [--cheat-menu] [--core-only]  install into ue4ss/Mods/CSSX (backs up what it replaces)
  cssx.py status                        print runtime/status.json and runtime/loader.json
  cssx.py request '{"op":"status"}'     dev channel request (needs Mods/CSSX/dev/enabled.txt)
                                        runtime ops name the extension with "extension", e.g. {"op":"model","extension":"eins0fx.cheat-menu"}
  cssx.py frame --seconds 10            frame statistics from the loader ring
  cssx.py quit                          ask the game to quit through Kismet QuitGame
  cssx.py disable | enable              toggle Mods/CSSX/enabled.txt (game closed)

Staging never touches Mods/CustomShellSystem. The loader (dlls/main.dll) is
only replaced while the game is closed; the core can be replaced while the
game runs (the loader watches core/ and switches on the game thread).
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent                       # extensions/core
REPO = ROOT.parents[1]                   # CustomShellSystem
GAME = Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2')
PIN = json.loads((REPO / 'native/ue4ss-runtime.json').read_text())
VERSION = (ROOT / 'VERSION').read_text().strip()


def sha(path: Path) -> str:
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def atomic(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + '.tmp')
    with temp.open('w', encoding='utf-8') as out:
        json.dump(value, out, indent=2)
        out.write('\n')
        out.flush()
        os.fsync(out.fileno())
    temp.replace(path)


def processes() -> list[int]:
    found = []
    for proc in Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            cmd = (proc / 'cmdline').read_bytes().split(b'\0')
            if cmd and cmd[0].replace(b'\\', b'/').endswith(b'/MortalShell2-Win64-Shipping.exe'):
                found.append(int(proc.name))
        except OSError:
            pass
    return found


def mod_root(game: Path) -> Path:
    return game / 'Binaries/Win64/ue4ss/Mods/CSSX'


def copy_verified(source: Path, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    if sha(source) != sha(target):
        raise RuntimeError(f'Copy verification failed: {target}')


def build(dev: bool) -> Path:
    out = ROOT / ('build/windows-dev' if dev else 'build/windows')
    subprocess.run(['cmake', '-S', str(ROOT), '-B', str(out), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                    '-DCMAKE_TOOLCHAIN_FILE=' + str(REPO / 'native/toolchain-clang-cl.cmake'), f'-DCSSX_DEV={"ON" if dev else "OFF"}'], check=True)
    subprocess.run(['cmake', '--build', str(out), '-j', '8'], check=True)
    for name in ('main.dll', 'cssx_core.dll', 'cheat_menu.dll', 'teleport.dll'):
        if not (out / name).is_file():
            raise RuntimeError(f'Build did not produce {name}')
    return out


def stage(game: Path, dev: bool, cheat_menu: bool, core_only: bool = False, performance: bool = False, teleport: bool = False) -> None:
    ue4ss = game / 'Binaries/Win64/ue4ss'
    if sha(ue4ss / 'UE4SS.dll') != PIN['dll_sha256']:
        raise RuntimeError('Installed UE4SS differs from the pinned runtime; refusing to stage')
    out = build(dev)
    mod = mod_root(game)
    running = bool(processes())
    stamp = time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())
    backup = ROOT / 'work/backups' / stamp
    loader_source = out / 'main.dll'
    loader_target = mod / 'dlls/main.dll'
    if loader_target.exists() and sha(loader_target) != sha(loader_source) and core_only:
        print('Loader differs from the build; kept the installed one (--core-only). Restage with the game closed to update it.')
        loader_source = loader_target
    if loader_target.exists() and sha(loader_target) != sha(loader_source):
        if running:
            raise RuntimeError('The loader changed and the game is running. Close the game to replace dlls/main.dll, or pass --core-only.')
        backup.mkdir(parents=True, exist_ok=True)
        copy_verified(loader_target, backup / 'main.dll')
    if not loader_target.exists() or sha(loader_target) != sha(loader_source):
        copy_verified(loader_source, loader_target)
    core_name = f'cssx_core-{VERSION}{"-dev" if dev else ""}-{sha(out / "cssx_core.dll")[:12]}.dll'
    core_target = mod / 'core' / core_name
    if not core_target.exists():
        copy_verified(out / 'cssx_core.dll', core_target)
    # Prune stale cores (keep the newest four so a rollback is one edit away).
    cores = sorted((mod / 'core').glob('cssx_core-*.dll'), key=lambda p: p.stat().st_mtime)
    for old in cores[:-4]:
        if old.name != core_name:
            old.unlink()
    for name in ('README.txt', 'THIRD_PARTY_NOTICES.txt'):
        copy_verified(ROOT / 'packaging' / name, mod / name)
    (mod / 'assets').mkdir(parents=True, exist_ok=True)
    for name in ('logo.png', 'banner.png'):
        copy_verified(ROOT / 'assets' / name, mod / 'assets' / name)
    (mod / 'extensions').mkdir(parents=True, exist_ok=True)
    (mod / 'runtime').mkdir(parents=True, exist_ok=True)
    if dev:
        (mod / 'dev').mkdir(parents=True, exist_ok=True)
        (mod / 'dev/enabled.txt').touch()
    if cheat_menu:
        target = mod / 'extensions/eins0fx.cheat-menu'
        target.mkdir(parents=True, exist_ok=True)
        for name in ('menu.json', 'LICENSE', 'THIRD_PARTY_NOTICES.txt', 'CREDITS.txt', 'README.txt'):
            copy_verified(REPO / 'extensions/cheat-menu' / name, target / name)
        (target / 'assets').mkdir(exist_ok=True)
        copy_verified(REPO / 'extensions/cheat-menu/assets/banner-v1.png', target / 'assets/banner-v1.png')
        dll_name = f'cheat_menu-{sha(out / "cheat_menu.dll")[:12]}.dll'
        if not (target / dll_name).exists():
            copy_verified(out / 'cheat_menu.dll', target / dll_name)
        for old in target.glob('cheat_menu-*.dll'):
            if old.name != dll_name:
                old.unlink()
        manifest = json.loads((REPO / 'extensions/cheat-menu/extension.json').read_text())
        manifest['entry'] = dll_name
        atomic(target / 'extension.json', manifest)
    if teleport:
        src = REPO / 'extensions/traverse'
        tid = json.loads((src / 'extension.json').read_text())['id']
        # remove any previous id folder for this source (the mod was renamed)
        for old_dir in ('eins0fx.teleport', 'eins0fx.traverse'):
            if old_dir != tid:
                import shutil as _sh
                _sh.rmtree(mod / 'extensions' / old_dir, ignore_errors=True)
        target = mod / 'extensions' / tid
        target.mkdir(parents=True, exist_ok=True)
        for name in ('menu.json', 'README.txt', 'THIRD_PARTY_NOTICES.txt'):
            copy_verified(src / name, target / name)
        (target / 'assets').mkdir(exist_ok=True)
        copy_verified(src / 'assets/panel.png', target / 'assets/panel.png')
        dll_name = f'teleport-{sha(out / "teleport.dll")[:12]}.dll'
        if not (target / dll_name).exists():
            copy_verified(out / 'teleport.dll', target / dll_name)
        for old in target.glob('teleport-*.dll'):
            if old.name != dll_name:
                old.unlink()
        manifest = json.loads((src / 'extension.json').read_text())
        manifest['entry'] = dll_name
        atomic(target / 'extension.json', manifest)
    if performance:
        target = mod / 'extensions/cssx.performance'
        target.mkdir(parents=True, exist_ok=True)
        for name in ('extension.json', 'main.lua', 'menu.json', 'README.txt'):
            copy_verified(ROOT / 'performance' / name, target / name)
    atomic(mod / 'core.json', {'file': core_name})
    atomic(mod / 'release.json', {'version': VERSION, 'dev': dev, 'loader_sha256': sha(loader_target), 'core': core_name,
                                  'core_sha256': sha(core_target), 'ue4ss_dll_sha256': PIN['dll_sha256'], 'staged_utc': stamp})
    (mod / 'enabled.txt').touch()
    print(f'Staged {core_name} into {mod}')
    if running:
        print('Game is running: the loader (if already installed) switches to the new core on its next tick; otherwise restart the game.')
        try:
            print(json.dumps(wait_json(mod / 'runtime/loader.json', lambda j: j.get('core') == core_name, 20), indent=2))
        except TimeoutError as error:
            print(f'No live acknowledgement: {error}')


def wait_json(path: Path, predicate, timeout: float):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            data = json.loads(path.read_text())
            if predicate(data):
                return data
        except (OSError, json.JSONDecodeError):
            pass
        time.sleep(0.1)
    raise TimeoutError(f'No matching acknowledgement in {path}')


def request(game: Path, value: dict, timeout: float = 20) -> dict:
    mod = mod_root(game)
    if not (mod / 'dev/enabled.txt').exists():
        raise RuntimeError('The dev channel is off: create Mods/CSSX/dev/enabled.txt and restart the game')
    rid = str(time.time_ns())
    atomic(mod / 'runtime/request.json', {**value, 'id': rid})   # 'extension' names the target; 'id' is the request id
    return wait_json(mod / 'runtime/response.json', lambda j: j.get('id') == rid, timeout)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game', type=Path, default=GAME)
    parser.add_argument('action', choices=['build', 'stage', 'status', 'request', 'frame', 'quit', 'disable', 'enable'])
    parser.add_argument('json', nargs='?')
    parser.add_argument('--dev', action='store_true')
    parser.add_argument('--cheat-menu', action='store_true')
    parser.add_argument('--performance', action='store_true', help='also stage the CSSX Performance extension')
    parser.add_argument('--teleport', action='store_true', help='also stage the CSSX Teleport extension')
    parser.add_argument('--core-only', action='store_true', help='stage: keep the installed loader')
    parser.add_argument('--seconds', type=float, default=10)
    args = parser.parse_args()
    mod = mod_root(args.game)
    if args.action == 'build':
        print(build(args.dev))
    elif args.action == 'stage':
        stage(args.game, args.dev, args.cheat_menu, args.core_only, args.performance, args.teleport)
    elif args.action == 'status':
        for name in ('loader', 'status'):
            path = mod / 'runtime' / f'{name}.json'
            print(path.read_text() if path.exists() else f'{name}: no live acknowledgement yet')
    elif args.action == 'request':
        print(json.dumps(request(args.game, json.loads(args.json or '{"op":"status"}')), indent=2))
    elif args.action == 'frame':
        print(json.dumps(request(args.game, {'op': 'frame.stats', 'seconds': args.seconds}), indent=2))
    elif args.action == 'quit':
        print(json.dumps(request(args.game, {'op': 'quit'}, timeout=5), indent=2))
    elif args.action in ('disable', 'enable'):
        if processes():
            raise SystemExit('Close the game first; UE4SS reads enabled.txt only at start')
        marker = mod / 'enabled.txt'
        if args.action == 'disable':
            marker.unlink(missing_ok=True)
        else:
            marker.touch()
        print(f'{marker} {"present" if marker.exists() else "absent"}')


if __name__ == '__main__':
    main()

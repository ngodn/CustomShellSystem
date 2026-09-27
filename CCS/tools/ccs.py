#!/usr/bin/env python3
"""Build, stage, and test Custom Combat System (CCS) for Mortal Shell II."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent                       # CCS root
REPO = ROOT.parent                      # CustomShellSystem
GAME = Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2')
MOD_DEST = GAME / 'Binaries/Win64/ue4ss/Mods/CCS'
PIN_FILE = REPO / 'native/ue4ss-runtime.json'
TOOLCHAIN = REPO / 'native/toolchain-clang-cl.cmake'


def sha(path: Path) -> str:
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def build(probe: bool = False, registry: bool = False, profile: bool = False, menu: bool = False, attack: bool = False, swap: bool = False) -> Path:
    probe = probe or registry
    if (menu and probe) or (attack and (menu or probe)) or (swap and (menu or probe or attack)):
        raise ValueError('Menu, discovery, attack and swap probes require separate builds')
    variant = 'windows-swap' if swap else 'windows-attack' if attack else 'windows-registry' if registry else 'windows-probe' if probe else 'windows-menu' if menu else 'windows'
    out = ROOT / 'build' / (variant + ('-profile' if profile else ''))
    out.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        'cmake', '-S', str(ROOT), '-B', str(out),
        '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        f'-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN}',
        f'-DCCS_FRAME_PROFILE={"ON" if profile else "OFF"}',
        f'-DCCS_DISCOVERY_PROBE={"ON" if probe else "OFF"}',
        f'-DCCS_ATTACK_PROBE={"ON" if attack else "OFF"}',
        f'-DCCS_SWAP_PROBE={"ON" if swap else "OFF"}',
        f'-DCCS_REGISTRY_CONTROLS={"ON" if registry else "OFF"}'
    ], check=True)
    subprocess.run(['cmake', '--build', str(out), '-j', '2'], check=True)

    for name in ('main.dll', 'ccs_core.dll'):
        if not (out / name).is_file():
            raise RuntimeError(f'Build did not produce {name}')
    print(f'[CCS] Build completed successfully in {out}')
    return out


def copy_verified(source: Path, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + '.tmp')
    shutil.copy2(source, temporary)
    if sha(source) != sha(temporary):
        raise RuntimeError(f'Copy verification failed: {target}')
    os.replace(temporary, target)


def game_processes() -> list[int]:
    result = []
    for proc in Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            command = (proc / 'cmdline').read_bytes().split(b'\0')
            if any(Path(arg.decode(errors='replace').replace('\\', '/')).name.lower() in
                   {'mortalshell2.exe', 'mortalshell2-win64-shipping.exe'} for arg in command):
                result.append(int(proc.name))
        except (OSError, ValueError):
            continue
    return result


BANNER = ROOT / 'assets/inventory-logo-1120x373-v2.png'


def stage_data() -> None:
    copy_verified(ROOT / 'data/catalog.json', MOD_DEST / 'catalog.json')
    for extra in ('enemy-catalog.json', 'ranged-catalog.json', 'running-catalog.json'):
        if (ROOT / 'data' / extra).is_file():
            copy_verified(ROOT / 'data' / extra, MOD_DEST / extra)
    if BANNER.is_file():
        (MOD_DEST / 'assets').mkdir(parents=True, exist_ok=True)
        copy_verified(BANNER, MOD_DEST / 'assets/banner.png')
    shipped = ROOT / 'presets'
    if shipped.is_dir():   # shipped presets are ours by name; a player's own presets have other names
        (MOD_DEST / 'presets').mkdir(parents=True, exist_ok=True)
        for preset in shipped.glob('*.json'):
            copy_verified(preset, MOD_DEST / 'presets' / preset.name)
    icons = ROOT / 'assets/enemy-icons'
    if icons.is_dir():
        (MOD_DEST / 'assets/enemy-icons').mkdir(parents=True, exist_ok=True)
        for icon in icons.glob('*.png'):
            copy_verified(icon, MOD_DEST / 'assets/enemy-icons' / icon.name)


def swap_core() -> None:
    """Build the product core and switch the running game to it through core.json (the loader polls it)."""
    out = build()
    (MOD_DEST / 'core').mkdir(parents=True, exist_ok=True)
    stage_data()
    version = (ROOT / 'VERSION').read_text().strip()
    core_name = f'ccs_core-{version}-{sha(out / "ccs_core.dll")[:12]}.dll'
    copy_verified(out / 'ccs_core.dll', MOD_DEST / 'core' / core_name)
    selector = MOD_DEST / 'core.json.tmp'
    selector.write_text(json.dumps({"file": core_name}, indent=2) + '\n')
    os.replace(selector, MOD_DEST / 'core.json')
    print(f'[CCS] core.json now selects {core_name}; the loader switches within a second')


def validate_pe(data: bytes, name: str) -> None:
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError(f'{name}: not a Windows DLL')
    offset = int.from_bytes(data[60:64], 'little')
    pe = data[offset:offset + 24]
    if pe[:6] != b'PE\0\0\x64\x86' or not int.from_bytes(pe[22:24], 'little') & 0x2000:
        raise ValueError(f'{name}: not an x64 DLL')


def release_files(version: str, out: Path) -> dict[str, bytes]:
    """Everything a player extracts: the installed layout under CCS/, nothing from a dev session."""
    core = f'ccs_core-{version}.dll'
    files = {
        'CCS/enabled.txt': b'',
        'CCS/dlls/main.dll': (out / 'main.dll').read_bytes(),
        f'CCS/core/{core}': (out / 'ccs_core.dll').read_bytes(),
        'CCS/core.json': (json.dumps({'file': core}, indent=2) + '\n').encode(),
        'CCS/README.txt': (ROOT / 'packaging/README.txt').read_text().replace('@VERSION@', version).encode(),
        'CCS/THIRD_PARTY_NOTICES.txt': (ROOT / 'packaging/THIRD_PARTY_NOTICES.txt').read_bytes(),
        'CCS/assets/banner.png': BANNER.read_bytes(),
    }
    for name in ('catalog.json', 'enemy-catalog.json', 'ranged-catalog.json', 'running-catalog.json'):
        files['CCS/' + name] = (ROOT / 'data' / name).read_bytes()
    for icon in sorted((ROOT / 'assets/enemy-icons').glob('*.png')):
        files['CCS/assets/enemy-icons/' + icon.name] = icon.read_bytes()
    return files


def verify_release(path: Path) -> dict:
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or archive.testzip():
            raise ValueError('Duplicate or damaged ZIP members')
        if 'CCS/release.json' not in names:
            raise ValueError('No release manifest')
        meta = json.loads(archive.read('CCS/release.json'))
        if set(meta['files']) | {'CCS/release.json'} != set(names):
            raise ValueError('ZIP members differ from the manifest')
        for name, expected in meta['files'].items():
            data = archive.read(name)
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError(f'Checksum mismatch: {name}')
            if name.endswith('.dll'):
                validate_pe(data, name)
            lower = name.lower()
            for forbidden in ('ue4ss.dll', '/logs/', '/runtime/', '/presets/', 'settings.json', '.pdb', '.log', 'swap-test'):
                if forbidden in lower:
                    raise ValueError(f'Forbidden member: {name}')
        return meta


def git(*args: str) -> str:
    return subprocess.run(['git', *args], cwd=REPO, check=True, capture_output=True, text=True).stdout.strip()


def release(output: Path | None = None) -> Path:
    """Build the product and write the player ZIP (extracts to ue4ss/Mods/) with a checksum manifest."""
    version = (ROOT / 'VERSION').read_text().strip()
    out = build()
    pin = json.loads(PIN_FILE.read_text())
    try:
        commit = git('rev-parse', 'HEAD')
        dirty = bool(git('status', '--porcelain', '--', str(ROOT)))
        stamp = time.gmtime(max(315532800, int(git('show', '-s', '--format=%ct', commit))))[:6]
    except (subprocess.CalledProcessError, FileNotFoundError):
        commit, dirty, stamp = 'unknown', True, time.gmtime()[:6]
    files = release_files(version, out)
    meta = {'product': 'CCS', 'version': version, 'source_commit': commit, 'dirty_tree': dirty, 'loader_core_abi': 1,
            'ue4ss_revision': pin['revision'], 'ue4ss_dll_sha256': pin['dll_sha256'],
            'files': {name: hashlib.sha256(data).hexdigest() for name, data in sorted(files.items())}}
    files['CCS/release.json'] = (json.dumps(meta, indent=2) + '\n').encode()
    target = (output or ROOT / 'dist' / f'ccs-v{version}') / f'MSII-CCS-v{version}.zip'
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=target.parent, prefix='ccs-release-') as temporary:
        candidate = Path(temporary) / target.name
        with zipfile.ZipFile(candidate, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name, data in sorted(files.items()):
                entry = zipfile.ZipInfo(name, stamp)
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, data)
        verify_release(candidate)
        os.replace(candidate, target)
    target.with_name(target.name + '.sha256').write_text(f'{sha(target)}  {target.name}\n')
    print(f'[CCS] Release written to {target} ({target.stat().st_size // 1024} KiB, {len(files)} files, commit {commit[:12]}{", dirty tree" if dirty else ""})')
    return target


def stage(confirmed_stopped: bool = False, probe: bool = False, registry: bool = False, attack: bool = False, swap: bool = False) -> None:
    probe = probe or registry
    if not confirmed_stopped:
        raise RuntimeError('Staging requires a fresh confirmation that you stopped playing (--confirm-game-stopped)')
    if game_processes():
        raise RuntimeError('Mortal Shell II is running; close it before staging CCS')
    pin = json.loads(PIN_FILE.read_text())
    if sha(GAME / 'Binaries/Win64/ue4ss/UE4SS.dll') != pin['dll_sha256']:
        raise RuntimeError('Installed UE4SS differs from the pinned runtime')
    out = build(probe=probe, registry=registry, attack=attack, swap=swap, profile=attack or swap)
    if game_processes():
        raise RuntimeError('The game started during the build; refusing to stage')
    MOD_DEST.mkdir(parents=True, exist_ok=True)
    (MOD_DEST / 'dlls').mkdir(parents=True, exist_ok=True)
    (MOD_DEST / 'core').mkdir(parents=True, exist_ok=True)
    (MOD_DEST / 'presets').mkdir(parents=True, exist_ok=True)

    # 1. enabled.txt
    (MOD_DEST / 'enabled.txt').touch(exist_ok=True)

    # 2. main.dll
    copy_verified(out / 'main.dll', MOD_DEST / 'dlls/main.dll')

    # 3. ccs_core.dll
    version = (ROOT / 'VERSION').read_text().strip()
    core_name = f'ccs_core-{version}-{sha(out / "ccs_core.dll")[:12]}.dll'
    copy_verified(out / 'ccs_core.dll', MOD_DEST / 'core' / core_name)

    # 4. core.json
    core_json = {"file": core_name}
    selector = MOD_DEST / 'core.json.tmp'
    selector.write_text(json.dumps(core_json, indent=2) + '\n')
    os.replace(selector, MOD_DEST / 'core.json')

    # 5. Move catalog and artwork the core reads at startup.
    stage_data()

    # 5b. Copy presets
    src_presets = ROOT / 'presets'
    if src_presets.exists():
        for preset_file in src_presets.glob('*.json'):
            target = MOD_DEST / 'presets' / preset_file.name
            if not target.exists():
                copy_verified(preset_file, target)

    # 6. Swap test mapping: copy the checked-in default only when the user has no edited copy.
    if swap:
        default_map = ROOT / 'work/swap-probe/swap-test.json'
        target = MOD_DEST / 'swap-test.json'
        if default_map.is_file() and not target.exists():
            copy_verified(default_map, target)

    print(f'[CCS] Staged successfully to {MOD_DEST}')


def main():
    parser = argparse.ArgumentParser(description='CCS build and stage tool')
    parser.add_argument('action', choices=['build', 'build-probe', 'build-registry', 'build-attack', 'build-swap', 'build-profile', 'stage', 'swap-core', 'release', 'status'], default='build', nargs='?')
    parser.add_argument('--confirm-game-stopped', action='store_true', help='Confirm you have stopped playing for this installation')
    parser.add_argument('--probe', action='store_true', help='Stage the read-only discovery variant')
    parser.add_argument('--registry', action='store_true', help='Stage the second discovery probe with registry controls')
    parser.add_argument('--menu', action='store_true', help='Include the experimental menu in build-profile')
    parser.add_argument('--attack', action='store_true', help='Select the read-only attack-call probe for stage or build-profile')
    parser.add_argument('--swap', action='store_true', help='Select the F7-armed montage swap test for stage')
    args = parser.parse_args()
    if args.menu and args.action != 'build-profile':
        parser.error('--menu is supported only by build-profile')
    if (args.probe or args.registry or args.attack or args.swap) and args.action not in {'stage', 'build-profile'}:
        parser.error('--probe, --registry, --attack and --swap are supported only by stage or build-profile')
    if (args.attack and (args.menu or args.probe or args.registry)) or (args.swap and (args.menu or args.probe or args.registry or args.attack)):
        parser.error('Menu, discovery, attack and swap probes require separate builds')

    if args.action == 'build':
        build()
    elif args.action == 'build-probe':
        build(probe=True)
    elif args.action == 'build-registry':
        build(registry=True)
    elif args.action == 'build-attack':
        build(attack=True, profile=True)
    elif args.action == 'build-swap':
        build(swap=True, profile=True)
    elif args.action == 'build-profile':
        build(probe=args.probe, registry=args.registry, profile=True, menu=args.menu, attack=args.attack, swap=args.swap)
    elif args.action == 'swap-core':
        swap_core()
    elif args.action == 'release':
        release()
    elif args.action == 'stage':
        stage(args.confirm_game_stopped, probe=args.probe, registry=args.registry, attack=args.attack, swap=args.swap)
    elif args.action == 'status':
        print(f'Mod destination: {MOD_DEST}')
        print(f'Exists: {MOD_DEST.exists()}')
        if (MOD_DEST / 'core.json').exists():
            print(f'core.json: {(MOD_DEST / "core.json").read_text()}')


if __name__ == '__main__':
    main()

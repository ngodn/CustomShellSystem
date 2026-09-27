#!/usr/bin/env python3
"""Build, stage, and test Custom Combat System (CCS) for Mortal Shell II."""
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
ROOT = HERE.parent                       # CCS root
REPO = ROOT.parent                      # CustomShellSystem
GAME = Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2')
MOD_DEST = GAME / 'Binaries/Win64/ue4ss/Mods/CCS'
PIN_FILE = REPO / 'native/ue4ss-runtime.json'
TOOLCHAIN = REPO / 'native/toolchain-clang-cl.cmake'


def sha(path: Path) -> str:
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def build() -> Path:
    out = ROOT / 'build/windows'
    out.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        'cmake', '-S', str(ROOT), '-B', str(out),
        '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        f'-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN}'
    ], check=True)
    subprocess.run(['cmake', '--build', str(out), '-j', '8'], check=True)

    for name in ('main.dll', 'ccs_core.dll'):
        if not (out / name).is_file():
            raise RuntimeError(f'Build did not produce {name}')
    print(f'[CCS] Build completed successfully in {out}')
    return out


def stage() -> None:
    out = build()
    MOD_DEST.mkdir(parents=True, exist_ok=True)
    (MOD_DEST / 'dlls').mkdir(parents=True, exist_ok=True)
    (MOD_DEST / 'core').mkdir(parents=True, exist_ok=True)
    (MOD_DEST / 'presets').mkdir(parents=True, exist_ok=True)

    # 1. enabled.txt
    (MOD_DEST / 'enabled.txt').touch(exist_ok=True)

    # 2. main.dll
    shutil.copy2(out / 'main.dll', MOD_DEST / 'dlls/main.dll')

    # 3. ccs_core.dll
    core_name = f'ccs_core-1.0.0-{sha(out / "ccs_core.dll")[:12]}.dll'
    shutil.copy2(out / 'ccs_core.dll', MOD_DEST / 'core' / core_name)

    # 4. core.json
    core_json = {"file": core_name}
    with (MOD_DEST / 'core.json').open('w') as f:
        json.dump(core_json, f, indent=2)

    # 5. Copy presets
    src_presets = ROOT / 'presets'
    if src_presets.exists():
        for preset_file in src_presets.glob('*.json'):
            shutil.copy2(preset_file, MOD_DEST / 'presets' / preset_file.name)

    print(f'[CCS] Staged successfully to {MOD_DEST}')


def main():
    parser = argparse.ArgumentParser(description='CCS build and stage tool')
    parser.add_argument('action', choices=['build', 'stage', 'status'], default='build', nargs='?')
    args = parser.parse_args()

    if args.action == 'build':
        build()
    elif args.action == 'stage':
        stage()
    elif args.action == 'status':
        print(f'Mod destination: {MOD_DEST}')
        print(f'Exists: {MOD_DEST.exists()}')
        if (MOD_DEST / 'core.json').exists():
            print(f'core.json: {(MOD_DEST / "core.json").read_text()}')


if __name__ == '__main__':
    main()

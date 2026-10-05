"""Install or remove CINE's asset pak in the game's ~mods folder. Python 3.14, standard library.

Usage: install.py PACK_DIRECTORY [--game PATH] [--remove]

Refuses while the game runs (a pak is mounted at start). Adds one new folder,
~mods/CSSX_CINE_eins0fx_P; replaces only its own earlier copy.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

GAME = Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2')
parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('pack', type=Path)
parser.add_argument('--game', type=Path, default=GAME)
parser.add_argument('--remove', action='store_true')
args = parser.parse_args()
for proc in Path('/proc').iterdir():
    if proc.name.isdigit():
        try:
            cmd = (proc / 'cmdline').read_bytes().split(b'\0')
            if cmd and cmd[0].replace(b'\\', b'/').endswith(b'/MortalShell2-Win64-Shipping.exe'):
                raise SystemExit('Close the game first; paks are mounted at start.')
        except OSError:
            pass
manifest = json.loads((args.pack / 'pack.json').read_text())
name = manifest['container']
target = args.game / 'Content/Paks/~mods' / name
if args.remove:
    shutil.rmtree(target, ignore_errors=True)
    print(f'removed {target}')
    raise SystemExit(0)
target.mkdir(parents=True, exist_ok=True)
for file, digest in manifest['files'].items():
    shutil.copy2(args.pack / file, target / file)
    if hashlib.sha256((target / file).read_bytes()).hexdigest() != digest:
        raise SystemExit(f'Copy verification failed: {file}')
print(f'installed {name} into {target}')

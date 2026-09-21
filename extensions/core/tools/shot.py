#!/usr/bin/env python3
"""Take a Steam screenshot through the CSSX dev channel and copy it under work/screens/.
Usage: shot.py NAME   (dev build only; the game keeps focus)"""
import shutil, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cssx import GAME, ROOT, request

SHOTS = Path.home() / '.local/share/Steam/userdata/238643500/760/remote/2584270/screenshots'
OUT = ROOT / 'work/screens'


def shot(name: str) -> Path:
    OUT.mkdir(parents=True, exist_ok=True)
    before = set(SHOTS.glob('*.jpg'))
    request(GAME, {'op': 'screenshot'})
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        new = set(SHOTS.glob('*.jpg')) - before
        if new:
            source = max(new, key=lambda p: p.stat().st_mtime)
            time.sleep(1.0)
            target = OUT / f'{name}.jpg'
            shutil.copyfile(source, target)
            print(target)
            return target
        time.sleep(0.25)
    raise TimeoutError('Steam screenshot did not arrive')


if __name__ == '__main__':
    shot(sys.argv[1])

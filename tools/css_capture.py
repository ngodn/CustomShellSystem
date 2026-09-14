#!/usr/bin/env python3
"""Local media driver for a CSS_INVENTORY_DEV core. Python 3.14.

Run only with the game focused and the character somewhere safe. Does not ship
with the mod. Requests use the same acknowledgements as inventory_dev.py.
"""
from pathlib import Path
import json
import shutil
import signal
import subprocess
import time
import threading
from contextlib import contextmanager
from css import GAME, ROOT, atomic, wait_json

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
WORK = ROOT / 'work/media-v0.2.0'
MEDIA = ROOT / 'docs/media/v0.2.0'
SHOTS = Path.home() / '.local/share/Steam/userdata/238643500/760/remote/2584270/screenshots'

def command(action, **fields):
    rid = str(time.time_ns())
    atomic(MOD / 'request.json', dict(id=rid, action=action, **fields))
    dev = action.startswith('inventory_')
    result = wait_json(MOD / ('runtime/inventory.json' if dev else 'runtime/status.json'),
                      lambda j: j.get('id' if dev else 'last_request') == rid)
    if result.get('error'): raise RuntimeError(result['error'])
    return result

def key(name):
    command('inventory_test_key', key=name, down=True)
    command('inventory_test_key', key=name, down=False)
    time.sleep(1)

def row(section, number=0, offset=0):
    command('inventory_capture_row', section=section, row=number, offset=offset)
    time.sleep(.8)

def motion(yaw=175, zoom=0, pan=0, frame=0, seconds=5):
    command('inventory_capture_motion', view=[yaw, zoom, pan, frame], seconds=seconds)
    time.sleep(seconds + .3)

def shot(name):
    MEDIA.mkdir(parents=True, exist_ok=True)
    before = set(SHOTS.glob('*.jpg'))
    command('inventory_shot')
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        new = set(SHOTS.glob('*.jpg')) - before
        if new:
            source = max(new, key=lambda p: p.stat().st_mtime)
            time.sleep(.8)
            target = MEDIA / (name + '.jpg')
            shutil.copyfile(source, target)
            target.chmod(0o644)
            print(target, flush=True)
            return target
        time.sleep(.2)
    raise TimeoutError('Steam screenshot did not arrive')

def click(action, **fields):
    info = command('inventory_inspect')
    hit = next(h for h in info['hit_points'] if h['action'].get('action') == action
               and all(h['action'].get(k) == v for k, v in fields.items()))
    command('inventory_test_pointer', point=hit['pixel'])
    command('inventory_test_mouse', event='left_down')
    command('inventory_test_mouse', event='left_up')
    time.sleep(.8)

@contextmanager
def recording(name):
    originals = WORK / 'originals'
    originals.mkdir(parents=True, exist_ok=True)
    target = originals / (name + '.mp4')
    if target.exists(): raise FileExistsError(target)
    # Screen capture includes the final displayed frame. The game must remain
    # fullscreen on DP-1 for the whole take; verify the workspace before starting.
    clients = json.loads(subprocess.check_output(['hyprctl', 'clients', '-j']))
    active = json.loads(subprocess.check_output(['hyprctl', 'activewindow', '-j']))
    game = next(c for c in clients if c.get('class') == 'steam_app_2584270'
                and c.get('title', '').strip() == 'MortalShell2')
    if active.get('address') != game['address'] or not game.get('fullscreen'):
        raise RuntimeError('Focus the fullscreen game before recording')
    with (originals / (name + '.log')).open('w') as log:
        process = subprocess.Popen(['gpu-screen-recorder', '-w', 'DP-1',
            '-f', '60', '-fm', 'cfr', '-q', 'very_high', '-k', 'h264',
            '-a', 'default_output', '-ac', 'aac', '-ab', '192',
            '-cursor', 'no', '-o', str(target)], stdout=log, stderr=log)
        stopped = threading.Event()
        interrupted = []
        def guard_display():
            while not stopped.wait(.1):
                try:
                    monitors = json.loads(subprocess.check_output(['hyprctl', 'monitors', '-j']))
                    display = next(m for m in monitors if m['name'] == 'DP-1')
                    current = json.loads(subprocess.check_output(['hyprctl', 'activewindow', '-j']))
                    if (display['activeWorkspace']['id'] != game['workspace']['id']
                            or current.get('address') != game['address']):
                        raise RuntimeError('Game lost focus during recording; reject this take')
                except Exception as error:
                    interrupted.append(str(error))
                    if process.poll() is None: process.send_signal(signal.SIGINT)
                    return
        guard = threading.Thread(target=guard_display, daemon=True)
        guard.start()
        try:
            time.sleep(1)
            if process.poll() is not None: raise RuntimeError('Recorder failed; check its log')
            yield target
        finally:
            stopped.set()
            guard.join(timeout=2)
            if process.poll() is None: process.send_signal(signal.SIGINT)
            process.wait(timeout=20)
        if interrupted: raise RuntimeError(interrupted[0])
        if process.returncode not in (0, 255): raise RuntimeError(f'Recorder exited {process.returncode}')

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['shot', 'command'])
    parser.add_argument('value')
    args = parser.parse_args()
    if args.action == 'shot': shot(args.value)
    else:
        request = json.loads(args.value)
        print(json.dumps(command(request.pop('action'), **request), indent=2))

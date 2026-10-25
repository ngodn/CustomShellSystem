"""Roll a stills preset and screenshot the game window once each held pose has settled. Python 3.14.

  stills.py PRESET OUT_DIR [--every SECONDS] [--settle SECONDS] [--count N]

The preset holds one pose every SECONDS (3 by default); each screenshot is taken SETTLE seconds
into its hold (2.2 by default), as a lossless PNG of the window's own rectangle (grim), so
nothing about how the game renders is changed. Checks her look is as it was afterwards.
"""
import argparse
import json
import subprocess
import time
from pathlib import Path

import reel as r
from record import window


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('preset')
    ap.add_argument('out')
    ap.add_argument('--every', type=float, default=3.0)
    ap.add_argument('--settle', type=float, default=2.2)
    ap.add_argument('--count', type=int, default=6)
    opts = ap.parse_args()
    game, shown = window()
    back = None
    if not shown:
        # A screenshot sees only what is displayed: show the game's workspace (the window keeps
        # its size and place) and put the screen back afterwards, as record.py does.
        monitors = json.loads(subprocess.check_output(['hyprctl', '-j', 'monitors']))
        on = next((m for m in monitors if m['id'] == game['monitor']), None)
        back = on['activeWorkspace']['name'] if on else None
        before = (game['at'], game['size'])
        subprocess.run(['hyprctl', 'dispatch', f'hl.dsp.focus({{ workspace = "{game["workspace"]["name"]}" }})'], capture_output=True)
        time.sleep(1.5)
        game, shown = window()
        if not shown or (game['at'], game['size']) != before:
            raise SystemExit('Could not show the game window unchanged; show its workspace and run this again.')
    (x, y), (w, h) = game['at'], game['size']
    out = Path(opts.out)
    out.mkdir(parents=True, exist_ok=True)
    own = r.css({'action': 'describe'})
    r.event('preset', opts.preset); r.event('breakdown', 'preset'); r.event('countdown', 0)
    if r.controls()[0]['enter'].get('enabled', True):
        r.event('enter'); time.sleep(0.6)
    r.event('take')
    began = time.monotonic()
    rolled_at = None
    while time.monotonic() - began < 60:
        status = r.controls()[0]['status']['value']
        if 'Rolling' in status:
            rolled_at = time.monotonic()
            break
        time.sleep(0.05)
    if rolled_at is None:
        raise SystemExit('The take did not roll: ' + status)
    shots = []
    for i in range(opts.count):
        wait = rolled_at + i * opts.every + opts.settle - time.monotonic()
        if wait > 0:
            time.sleep(wait)
        path = out / f'{opts.preset}-{i + 1}.png'
        # The game must still be what the screen shows, or the shot is of something else.
        now_game, now_shown = window()
        if not now_shown or (now_game['at'], now_game['size']) != (game['at'], game['size']):
            print(f'still {i + 1} skipped: the game window is not on screen')
            continue
        subprocess.run(['grim', '-g', f'{x},{y} {w}x{h}', str(path)], check=True)
        shots.append(path)
        print(path)
    while time.monotonic() - began < 120:
        status = r.controls()[0]['status']['value']
        if any(word in status for word in ('finished', 'cancel', 'stopped', 'Could not')):
            break
        time.sleep(0.5)
    now = own
    for _ in range(24):
        time.sleep(0.5)
        now = r.css({'action': 'describe'})
        if r.same_look(now, own):
            break
    print('her look restored:', r.same_look(now, own))
    if back:
        subprocess.run(['hyprctl', 'dispatch', f'hl.dsp.focus({{ workspace = "{back}" }})'], capture_output=True)


if __name__ == '__main__':
    main()

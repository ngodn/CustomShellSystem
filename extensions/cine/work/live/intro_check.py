"""Roll a preset, photograph its first seconds, then leave Cine World and check her look. Python 3.14.

  intro_check.py PRESET [seconds] [seconds between screenshots]
"""
import json
import sys
import time

import reel as r


def main():
    preset = sys.argv[1]
    length = float(sys.argv[2]) if len(sys.argv) > 2 else 19.5
    every = float(sys.argv[3]) if len(sys.argv) > 3 else 1.25
    for old in r.OUT.glob('reel-t*.jpg'):
        old.unlink()
    own = r.css({'action': 'describe'})
    (r.OUT / 'own-look.json').write_text(json.dumps(own))
    r.event('preset', preset); r.event('breakdown', 'preset'); r.event('countdown', 0); r.event('guide', 'off')
    if r.controls()[0]['enter'].get('enabled', True):
        r.event('enter'); time.sleep(0.6)
    r.event('take')
    rolled = None
    for _ in range(150):
        time.sleep(0.2)
        if 'Rolling' in r.controls()[0]['status']['value']:
            rolled = time.monotonic()
            break
    shots = 0
    while rolled and time.monotonic() - rolled < length:
        shots += r.grab(f't{time.monotonic() - rolled:05.1f}')
        time.sleep(every)
    r.event('exit')
    time.sleep(2.5)
    now = r.css({'action': 'describe'})
    for _ in range(24):
        if r.same_look(now, own):
            break
        time.sleep(0.5)
        now = r.css({'action': 'describe'})
    print('shots', shots, '| restored:', r.same_look(now, own), '| status:', r.controls()[0]['status']['value'])


if __name__ == '__main__':
    main()

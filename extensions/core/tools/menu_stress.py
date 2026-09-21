#!/usr/bin/env python3
"""Open and close the CSSX tab repeatedly through the hotkey path and watch for leaks.

  menu_stress.py [--cycles 30]

Per cycle: hotkey (opens the Player Menu on the CSSX tab), open the first
extension, walk two sections, hotkey (closes). Reports widget counts, build
cost and frame statistics before and after. Nothing is granted or applied.
"""
import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cssx import GAME, request  # noqa: E402


def rt(value):
    r = request(GAME, value)
    if not r['ok']:
        raise RuntimeError(r.get('error', str(r)))
    return r['result']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cycles', type=int, default=30)
    args = parser.parse_args()
    before = rt({'op': 'frame.stats', 'seconds': 10})
    failures = 0
    t0 = time.monotonic()
    for i in range(args.cycles):
        rt({'op': 'menu.hotkey'}); time.sleep(0.6)
        d = rt({'op': 'menu.diagnostics'})
        if not d['open']:
            failures += 1
            print(f'cycle {i}: menu did not open ({d})')
            rt({'op': 'menu.close'}); time.sleep(0.4); continue
        rt({'op': 'menu.key', 'key': 'accept'}); time.sleep(0.3)
        rt({'op': 'menu.key', 'key': 'next_section'}); time.sleep(0.2)
        rt({'op': 'menu.key', 'key': 'next_section'}); time.sleep(0.2)
        rt({'op': 'menu.key', 'key': 'close'}); time.sleep(0.2)
        rt({'op': 'menu.hotkey'}); time.sleep(0.5)
        d = rt({'op': 'menu.diagnostics'})
        if d['open']:
            failures += 1
            print(f'cycle {i}: menu did not close ({d})')
            rt({'op': 'menu.close'}); time.sleep(0.4)
    elapsed = time.monotonic() - t0
    status = rt({'op': 'status'})
    after = rt({'op': 'frame.stats', 'seconds': 10})
    print(json.dumps({'cycles': args.cycles, 'failures': failures, 'seconds': round(elapsed, 1),
                      'menu_builds': after.get('menu_builds'), 'menu_open_after': status['menu_open'], 'game_menu_open_after': status['game_menu_open'],
                      'engine_before': {k: round(before['engine'][k], 2) for k in ('median_ms', 'p99_ms', 'hz')},
                      'engine_after': {k: round(after['engine'][k], 2) for k in ('median_ms', 'p99_ms', 'hz')}}, indent=2))
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Hitch attribution from the loader frame ring.

  hitches.py [--seconds 60] [--samples 1] [--interval 30]

Prints frame statistics and, for every frame above twice the median, how much
of that frame CSSX itself spent (core tick, extensions, menu, HUD). A hitch is
CSSX's when the core share is a large part of the frame; otherwise it belongs
to the game or another mod.
"""
import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cssx import GAME, request  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=60)
    parser.add_argument('--samples', type=int, default=1)
    parser.add_argument('--interval', type=float, default=30)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    out = []
    for i in range(args.samples):
        r = request(GAME, {'op': 'frame.stats', 'seconds': args.seconds})
        if not r['ok']:
            raise SystemExit(r.get('error'))
        s = r['result']
        e = s['engine']; h = s.get('hitches', {}); p = s['phases']['core_tick']
        print(f"[{time.strftime('%H:%M:%S')}] {e['count']} frames, median {e['median_ms']:.2f} ms, p99 {e['p99_ms']:.2f} ms, max {e['max_ms']:.1f} ms, {e['hz']:.1f} fps; "
              f"CSSX mean {p['mean_us']:.0f} us, max {p['max_us']:.0f} us; hitches {h.get('count')}, CSSX share at hitches max {h.get('core_share_max_us', 0):.0f} us, "
              f"frames where CSSX > 1/4 of the frame: {h.get('frames_where_core_exceeds_quarter')}")
        for w in h.get('worst', [])[:8]:
            print(f"   frame {w['frame_ms']:7.1f} ms   CSSX {w['core_us']:6.0f} us   ({w['age_frames']} frames ago)")
        out.append(s)
        if i + 1 < args.samples:
            time.sleep(args.interval)
    if args.json:
        args.json.write_text(json.dumps(out, indent=2))


if __name__ == '__main__':
    main()

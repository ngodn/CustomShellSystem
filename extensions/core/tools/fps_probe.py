#!/usr/bin/env python3
"""Frame-time capture and A/B comparison through the CSSX dev channel.

  fps_probe.py capture --label C-core-only --seconds 20 --warmup 10 [--pin-yaw 0]
      Records one capture into work/perf/<label>-<n>.json using frame.stats
      (loader ring: engine tick intervals, game thread). Optionally pins the
      player's control rotation first so repeated captures share a view.
  fps_probe.py noise LABEL...            spread across repeats of the same label
  fps_probe.py compare BASE CANDIDATE    pairwise comparison with the noise floor
  fps_probe.py review FILE               print one capture

Engine cadence is not GPU presentation timing. Each row of the comparison
table in docs/performance.md is a normal restart; this tool never changes
graphics settings, frame caps or gameplay content.
"""
from __future__ import annotations
import argparse
import json
import statistics
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cssx import GAME, ROOT, processes, request  # noqa: E402

WORK = ROOT / 'work/perf'


def capture(game: Path, label: str, seconds: float, warmup: float, pin_yaw: float | None) -> Path:
    pids = processes()
    if len(pids) != 1:
        raise SystemExit('Expected exactly one running game')
    status = request(game, {'op': 'status'})['result']
    if not status.get('player'):
        raise SystemExit('No player pawn: load into the world first')
    if pin_yaw is not None:
        player = request(game, {'op': 'engine', 'request': {'op': 'player'}})['result']
        request(game, {'op': 'engine', 'request': {'op': 'call', 'target': player['controller'], 'function': 'SetControlRotation',
                                                    'args': {'NewRotation': {'Pitch': -10.0, 'Yaw': pin_yaw, 'Roll': 0.0}}}})
    time.sleep(warmup)
    before = request(game, {'op': 'frame.stats', 'seconds': 1})['result']
    time.sleep(seconds)
    stats = request(game, {'op': 'frame.stats', 'seconds': seconds})['result']
    after_status = request(game, {'op': 'status'})['result']
    WORK.mkdir(parents=True, exist_ok=True)
    n = len(list(WORK.glob(f'{label}-*.json'))) + 1
    out = WORK / f'{label}-{n}.json'
    out.write_text(json.dumps({'label': label, 'seconds': seconds, 'warmup': warmup, 'pin_yaw': pin_yaw, 'pid': pids[0],
                               'status_before': status, 'status_after': after_status, 'frames_total_before': before.get('frames_total'),
                               'stats': stats, 'captured_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}, indent=2) + '\n')
    e = stats['engine']
    print(f'{out.name}: {e["count"]} frames, median {e["median_ms"]:.2f} ms, p95 {e["p95_ms"]:.2f} ms, p99 {e["p99_ms"]:.2f} ms, {e["hz"]:.1f} Hz, hitches {e["hitches"]}')
    return out


def load(label: str) -> list[dict]:
    files = sorted(WORK.glob(f'{label}-*.json'))
    if not files:
        raise SystemExit(f'No captures for {label} under {WORK}')
    return [json.loads(f.read_text()) for f in files]


def medians(captures: list[dict]) -> list[float]:
    return [c['stats']['engine']['median_ms'] for c in captures]


def noise(labels: list[str]) -> dict:
    """Noise floor: largest spread of median frame time across repeats of one configuration."""
    result = {}
    for label in labels:
        m = medians(load(label))
        result[label] = {'repeats': len(m), 'medians_ms': m, 'spread_ms': max(m) - min(m) if len(m) > 1 else None,
                         'stdev_ms': statistics.stdev(m) if len(m) > 1 else None}
    return result


def compare(base: str, candidate: str) -> dict:
    b, c = load(base), load(candidate)
    bm, cm = medians(b), medians(c)
    floor = max(max(bm) - min(bm), max(cm) - min(cm)) if len(bm) > 1 and len(cm) > 1 else None
    delta = statistics.median(cm) - statistics.median(bm)
    bp99 = statistics.median(x['stats']['engine']['p99_ms'] for x in b)
    cp99 = statistics.median(x['stats']['engine']['p99_ms'] for x in c)
    verdict = 'inconclusive'
    if floor is not None and len(bm) >= 3 and len(cm) >= 3:
        if abs(delta) <= floor and cp99 <= bp99 * 1.5:
            verdict = 'within noise'
        elif delta > floor:
            verdict = 'candidate slower'
        elif delta < -floor:
            verdict = 'candidate faster'
        else:
            verdict = 'median within noise, p99 regressed'
    return {'base': base, 'candidate': candidate, 'base_medians_ms': bm, 'candidate_medians_ms': cm, 'noise_floor_ms': floor,
            'median_delta_ms': delta, 'base_p99_ms': bp99, 'candidate_p99_ms': cp99, 'pairs': min(len(bm), len(cm)), 'verdict': verdict,
            'scope': 'Engine tick cadence on the game thread; same save/view assumed by the operator.'}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game', type=Path, default=GAME)
    sub = parser.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('capture'); c.add_argument('--label', required=True); c.add_argument('--seconds', type=float, default=20); c.add_argument('--warmup', type=float, default=10); c.add_argument('--pin-yaw', type=float)
    n = sub.add_parser('noise'); n.add_argument('labels', nargs='+')
    p = sub.add_parser('compare'); p.add_argument('base'); p.add_argument('candidate')
    r = sub.add_parser('review'); r.add_argument('file', type=Path)
    args = parser.parse_args()
    if args.cmd == 'capture':
        if not 3 <= args.seconds <= 60:
            parser.error('seconds must be 3 to 60')
        capture(args.game, args.label, args.seconds, args.warmup, args.pin_yaw)
    elif args.cmd == 'noise':
        print(json.dumps(noise(args.labels), indent=2))
    elif args.cmd == 'compare':
        print(json.dumps(compare(args.base, args.candidate), indent=2))
    else:
        data = json.loads(args.file.read_text())
        print(json.dumps(data['stats'], indent=2))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Capture bounded developer-core timings without input or screen capture.

Python 3.14. Run only during an authorized live test. No deployment is performed.
"""
import argparse
import json
import math
from pathlib import Path
import statistics
import time
from css import ROOT, GAME, atomic, processes, sha

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


def summarize(report):
    rows = report['rows']
    if len(rows) < 30:
        raise ValueError('Too few frames for a sustained-FPS comparison')
    if any(row['failed'] for row in rows):
        raise ValueError('CSS threw during capture; inspect the saved raw report')
    columns = {key: [row[key] for row in rows] for key in ('interval_ms', 'engine_ms', 'core_ms')}
    phases = report['phases']
    if phases != ['recovery', 'cssx_tick', 'hud_prepare', 'cssx_render', 'inventory']:
        raise ValueError('Unexpected phase layout')
    for row in rows:
        if len(row['phase_ms']) != len(phases):
            raise ValueError('Incomplete phase measurements')
        if sum(row['phase_ms']) > row['core_ms'] + .05:
            raise ValueError('Phase timings exceed the enclosing core tick')
    for index, name in enumerate(phases):
        columns[name + '_ms'] = [row['phase_ms'][index] for row in rows]
    if any(not math.isfinite(value) or value < 0 for values in columns.values() for value in values):
        raise ValueError('Invalid timing sample')
    if any(value <= 0 for value in columns['interval_ms']):
        raise ValueError('Missing wall interval; repeat after normal warmup')
    metrics = {}
    for name, values in columns.items():
        ordered = sorted(values)
        metrics[name] = {'mean': statistics.fmean(values), 'p50': statistics.median(values),
                         'p95': ordered[math.ceil(.95*len(ordered))-1], 'max': max(values)}
    return {'frames': len(rows), 'stop_reason': report['stop_reason'],
            'cssx_loaded': report['cssx_loaded'],
            'engine_tick_rate_hz': 1000 / metrics['interval_ms']['mean'],
            'metrics': metrics,
            'scope': 'Engine tick cadence and measured CSS phases. Not GPU or display presentation timing. '
                     'CSSX hooks called elsewhere in the frame are outside these phase timers.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core', type=Path, default=ROOT/'build/windows/css_core.dll')
    parser.add_argument('--review', type=Path, help='Review a saved raw report without contacting the game')
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT/'work') or output.exists():
        parser.error('Choose a new output file under CustomShellSystem/work')
    if not math.isfinite(args.seconds) or not 3 <= args.seconds <= 30:
        parser.error('Duration must be 3 to 30 seconds')
    if args.review:
        saved = json.loads(args.review.read_text())
        result = {'raw': saved.get('raw', saved)}
    else:
        pids = processes()
        if len(pids) != 1:
            raise RuntimeError('Expected exactly one running game')
        loader_path = MOD/'runtime/loader.json'
        loader = json.loads(loader_path.read_text())
        installed = (MOD/'cores'/loader['core']).resolve()
        if not installed.is_relative_to((MOD/'cores').resolve()) or sha(installed) != sha(args.core):
            raise RuntimeError('Loaded core differs from the specified instrumented build; nothing was sent')
        rid = 'frame-profile-' + str(time.time_ns())
        state = MOD/'state/state.json'
        state_sha = sha(state)
        atomic(MOD/'request.json', {'id': rid, 'action': 'frame_profile', 'seconds': args.seconds})
        deadline = time.monotonic() + args.seconds + 15
        while time.monotonic() < deadline:
            if processes() != pids or json.loads(loader_path.read_text())['core'] != loader['core']:
                raise RuntimeError('Game or core changed during capture')
            try:
                report = json.loads((MOD/'runtime/frame-profile.json').read_text())
                if report.get('id') == rid:
                    break
            except (FileNotFoundError, json.JSONDecodeError):
                pass
            time.sleep(.2)
        else:
            raise TimeoutError('No frame-profile result from the developer core')
        result = {'core': loader['core'], 'core_sha256': sha(installed), 'pids': pids,
                  'state_unchanged': state_sha == sha(state), 'raw': report}
    output.parent.mkdir(parents=True, exist_ok=True)
    # Keep measured evidence even when validation fails.
    output.write_text(json.dumps(result, indent=2) + '\n')
    result['summary'] = summarize(result['raw'])
    output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    print(json.dumps(result['summary'], indent=2))


if __name__ == '__main__':
    main()

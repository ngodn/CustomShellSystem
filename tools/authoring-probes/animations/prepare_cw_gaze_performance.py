"""Prepare full CW source-rig samples for an audited gaze/blink/phoneme clip."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from cw_eye_mapping import blended_direction, solve_gaze


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--mapping', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--clip', default='css.eve.dance.jangwonyoungspin')
    parser.add_argument('--refine-report', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    protected = {}

    def protect(path):
        path = path.resolve(strict=True)
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        if protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Source changed: ' + str(path))
        return path

    mapping = json.loads(protect(args.mapping).read_text())
    if not mapping.get('passed'):
        raise ValueError('Require verified source eye calibration')
    for path, checksum in mapping['protected'].items():
        if protected[str(protect(Path(path)))] != checksum:
            raise ValueError('Calibration input no longer matches: ' + path)
    manifest = json.loads(protect(args.catalog).read_text())
    face = next(r for r in manifest['catalog']['animation_library']['faces'] if r['id'] == 'css.eve.face')
    clip = next(p for p in face['performances'] if p['clip'] == args.clip)
    curves = {curve['morph']: np.asarray(curve['keys'], dtype=float) for curve in clip['curves']}
    names = {f'eye_{direction}_{side}' for side in ('L', 'R')
             for direction in ('left', 'right', 'up', 'down')}
    names.update(('blink', 'woo', 'ooo'))
    if set(curves) != names or len(curves) != len(clip['curves']):
        raise ValueError('Clip contains curves without an audited mapping')
    duration = float(clip['duration'])
    if not np.isfinite(duration) or not 0 < duration <= 8:
        raise ValueError('This source-sampling plan is bounded to eight seconds')
    for name, keys in curves.items():
        if (keys.ndim != 2 or keys.shape[1] != 2 or len(keys) < 2 or
                not np.isfinite(keys).all() or np.any(np.diff(keys[:, 0]) <= 0) or
                keys[0, 0] < 0 or keys[-1, 0] > duration or
                keys[:, 1].min() < 0 or keys[:, 1].max() > 1):
            raise ValueError('Unexpected source keys: ' + name)
    times = sorted(set(np.linspace(0, duration, int(np.ceil(duration*60)) + 1)) |
                   {float(key[0]) for keys in curves.values() for key in keys})
    if args.refine_report:
        previous = json.loads(protect(args.refine_report).read_text())
        if previous['clip'] != args.clip or len(previous['holdouts']) != len(times)-1:
            raise ValueError('Refinement report does not match this source timeline')
        extra = []
        for a, b, case in zip(times[:-1], times[1:], previous['holdouts'], strict=True):
            if abs(case['seconds']-(a+b)/2) > 1e-12:
                raise ValueError('Refinement interval differs')
            divisions = 8 if case['error_cm'] > .01 else 4 if case['error_cm'] > .004 else 1
            extra.extend(np.linspace(a, b, divisions+1)[1:-1])
        times = sorted(set(times) | set(extra))
    if len(times) > 512:
        raise ValueError('Source sampling exceeds the existing batch bound')

    def pose(seconds, index, prefix):
        source = {name: float(np.interp(seconds, keys[:, 0], keys[:, 1])) for name, keys in curves.items()}
        target = {'facs_ctrl_EyesBlink': source['blink'],
                  'facs_ctrl_vUW': source['woo'], 'facs_ctrl_vOW': source['ooo']}
        directions = {}
        for side, letter in [('Left', 'L'), ('Right', 'R')]:
            calibration = mapping['commanderwhite'][side]
            eye_names = [f'eye_{d}_{letter}' for d in ('left', 'right', 'up', 'down')]
            rotations = [np.asarray(mapping['source_rotations'][name]['rotation']) for name in eye_names]
            direction = blended_direction(rotations, [source[name] for name in eye_names])
            values = solve_gaze([np.asarray(a) for a in calibration['axes']], calibration['order'], direction)
            if np.abs(values).max() >= 1.49:
                raise ValueError('Mapped gaze exceeds the measured bake range')
            target.update(zip(calibration['controls'], values.tolist(), strict=True))
            directions[side] = direction.tolist()
        return dict(label=f'mix_{prefix}_{index:04}', seconds=seconds, controls=target,
                    source_weights=source, gaze=directions)

    samples = [pose(t, i, 'frame') for i, t in enumerate(times)]
    holdouts = [pose((a+b)/2, i, 'holdout') for i, (a, b) in enumerate(zip(times, times[1:]))]
    for path in list(protected):
        protect(Path(path))
    args.output.mkdir(parents=True)
    for filename, rows in [('combinations.json', samples), ('holdouts.json', holdouts)]:
        plan = [dict(label=row['label'], controls=row['controls']) for row in rows]
        (args.output/filename).write_text(json.dumps(plan, indent=2, allow_nan=False)+'\n')
    report = dict(prepared=True, protected=protected, clip=args.clip, duration=duration,
                  samples=samples, holdouts=holdouts,
                  mapping=dict(blink='facs_ctrl_EyesBlink', woo='facs_ctrl_vUW', ooo='facs_ctrl_vOW'),
                  scope='Preserves curve timing and measured gaze. Native CW phonemes preserve her proportions; combined geometry, visual and runtime acceptance remain separate.')
    (args.output/'plan.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print('CW_GAZE_PLAN', args.clip, len(samples), len(holdouts), flush=True)


if __name__ == '__main__':
    main()

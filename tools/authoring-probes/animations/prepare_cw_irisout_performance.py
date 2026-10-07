"""Map the full Iris Out face track to reviewed CW tongue, mouth and gaze actions."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from cw_eye_mapping import blended_direction, solve_gaze


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--gaze', type=Path, required=True)
    parser.add_argument('--eyelids', type=Path, required=True)
    parser.add_argument('--review', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--refine-report', type=Path, action='append', default=[])
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    protected = {}

    def read(path):
        path = path.resolve(strict=True)
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        if protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Input changed: '+str(path))
        return json.loads(path.read_text())

    gaze, eyelids, review = (read(p) for p in (args.gaze, args.eyelids, args.review))
    if not gaze.get('passed') or not eyelids.get('prepared') or not review.get('passed'):
        raise ValueError('Require verified gaze, measured eyelids and reviewed mouth mapping')
    for audit in (gaze, eyelids, review):
        for path, checksum in audit['protected'].items():
            with Path(path).open('rb') as stream:
                actual = hashlib.file_digest(stream, 'sha256').hexdigest()
            if actual != checksum or protected.setdefault(path, checksum) != checksum:
                raise ValueError('Calibration source changed: '+path)
    gains = review['gains']
    expected_gains = {'tongue_right', 'tongue_arch', 'tongue_wide', 'tongue_tip_bend',
                      'lower_lip_down', 'tongue_tip_extension'}
    if set(gains) != expected_gains or not all(np.isfinite(v) and -1 <= v <= 1 for v in gains.values()):
        raise ValueError('Unexpected reviewed control gains')
    if gains['tongue_tip_extension'] < 0:
        raise ValueError('Tongue extension must preserve forward motion')
    document = read(args.catalog)
    face = next(f for f in document['catalog']['animation_library']['faces'] if f['id'] == 'css.eve.face')
    clip = next(c for c in face['performances'] if c['clip'] == 'css.eve.dance.irisout')
    curves = {c['morph']: np.asarray(c['keys'], dtype=float) for c in clip['curves']}
    names = {'jaw_open', 'tongue_right', 'tongue_out', 'tongue_wide', 'tongue_arch',
             'tongueTip_out', 'lowerLip_down', 'dimple', 'suck', 'puff', 'lipStick'}
    names.update(f'eye_{d}_{side}' for side in ('L', 'R') for d in ('left', 'right', 'up', 'down'))
    names.update(f'{action}_{side}' for side in ('L', 'R') for action in ('upLid_up', 'lid_mid_closed'))
    duration = float(clip['duration'])
    if set(curves) != names or len(curves) != len(clip['curves']) or not np.isfinite(duration) or not 0 < duration <= 600:
        raise ValueError('Unexpected Iris Out source channels or duration')
    for name, keys in curves.items():
        if (keys.ndim != 2 or keys.shape[1] != 2 or len(keys) < 2 or not np.isfinite(keys).all() or
                np.any(np.diff(keys[:, 0]) <= 0) or keys[0, 0] < 0 or keys[-1, 0] > duration or
                keys[:, 1].min() < 0 or keys[:, 1].max() > 1):
            raise ValueError('Invalid source keys: '+name)

    def pose(t, index, kind):
        source = {name: float(np.interp(t, keys[:, 0], keys[:, 1])) for name, keys in curves.items()}
        extension = gains['tongue_tip_extension']
        target = {
            'facs_jnt_JawOpen': source['jaw_open'],
            'facs_ctrl_MouthLowerDown': source['lowerLip_down']*gains['lower_lip_down'],
            'facs_ctrl_MouthDimple': source['dimple'],
            'facs_ctrl_CheekPuff': source['puff']-source['suck'],
            'facs_ctrl_MouthPress': source['lipStick'],
            'facs_bs_MouthPucker_div2': source['lipStick']+source['suck'],
            'facs_bs_TongueOut': (source['tongue_out']+extension*source['tongueTip_out'])/(1+extension),
            'facs_bs_TongueNarrow': source['tongue_wide']*gains['tongue_wide'],
            'facs_jnt_TongueRight': source['tongue_right']*gains['tongue_right'],
            'facs_jnt_TongueUp': source['tongue_arch']*gains['tongue_arch'],
            'facs_jnt_TongueTipBend': source['tongueTip_out']*gains['tongue_tip_bend'],
        }
        directions = {}
        for side, letter in [('Left', 'L'), ('Right', 'R')]:
            calibration = gaze['commanderwhite'][side]
            target['facs_jnt_EyeBlink'+side] = source['lid_mid_closed_'+letter]*eyelids[
                'eyelid_calibration'][letter]['actions']['lid_mid_closed_'+letter]['gain']
            target['facs_jnt_EyesWide'+side] = source['upLid_up_'+letter]
            eye_names = [f'eye_{d}_{letter}' for d in ('left', 'right', 'up', 'down')]
            rotations = [np.asarray(gaze['source_rotations'][name]['rotation']) for name in eye_names]
            direction = blended_direction(rotations, [source[name] for name in eye_names])
            values = solve_gaze([np.asarray(a) for a in calibration['axes']], calibration['order'], direction)
            if np.abs(values).max() >= 1.49:
                raise ValueError('Gaze exceeds the audited bake range')
            target.update(zip(calibration['controls'], values.tolist(), strict=True))
            directions[side] = direction.tolist()
        if not all(np.isfinite(v) and -1.5 <= v <= 1.5 for v in target.values()):
            raise ValueError('Native action exceeds its sampled range')
        return dict(label=f'mix_{kind}_{index:05}', seconds=float(t), source_weights=source,
                    controls=target, gaze=directions)

    source_times = sorted({float(k[0]) for keys in curves.values() for k in keys} | {0., duration})
    times = sorted(set(source_times) | set(np.linspace(0, duration, int(np.ceil(duration*60))+1)))
    for path in args.refine_report:
        previous = read(path)
        if previous['clip'] != clip['clip'] or len(previous['holdouts']) != len(times)-1:
            raise ValueError('Refinement source differs')
        extra = []
        for a, b, row in zip(times[:-1], times[1:], previous['holdouts'], strict=True):
            if abs(row['seconds']-(a+b)/2) > 1e-12:
                raise ValueError('Refinement interval differs')
            divisions = 8 if row['error_cm'] > .01 else 4 if row['error_cm'] > .004 else 1
            extra.extend(np.linspace(a, b, divisions+1)[1:-1])
        times = sorted(set(times) | set(extra))
    if len(times) > 16384:
        raise ValueError('Source plan exceeds the sample bound')
    samples = [pose(t, i, 'frame') for i, t in enumerate(times)]
    holdouts = [pose((a+b)/2, i, 'holdout') for i, (a, b) in enumerate(zip(times, times[1:]))]
    for path, checksum in protected.items():
        with Path(path).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != checksum:
                raise ValueError('Input changed during preparation: '+path)
    args.output.mkdir(parents=True)
    for name, rows in [('combinations', samples), ('holdouts', holdouts)]:
        data = [dict(label=r['label'], controls=r['controls']) for r in rows]
        (args.output/(name+'.json')).write_text(json.dumps(data, indent=2, allow_nan=False)+'\n')
    report = dict(prepared=True, protected=protected, clip=clip['clip'], duration=duration,
                  samples=samples, holdouts=holdouts, source_key_times=source_times,
                  eye_gaze_range=True, tongue_fk=True, signed_mouth_range=True,
                  reviewed_gains=gains,
                  scope='Full source facial-action adaptation. Combined geometry, fitted export, cooking and live acceptance remain pending.')
    (args.output/'plan.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print('CW_IRISOUT_PLAN', len(samples), len(holdouts), flush=True)


if __name__ == '__main__':
    main()

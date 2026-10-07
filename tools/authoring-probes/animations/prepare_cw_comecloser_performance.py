"""Prepare all Come Closer facial source keys for CW's reviewed native controls."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def mapped_controls(source, eyelids, gains):
    target = {
        'facs_jnt_JawOpen': source['jaw_open'],
        'facs_jnt_TongueRight': gains['tongue_right']*(source['tongue_right']-source['tongue_left']),
        'facs_jnt_TongueUp': source['tongue_up']+gains['tongue_arch']*source['tongue_arch'],
        'facs_bs_TongueOut': gains['tongue_out']*source['tongue_out'],
        'facs_jnt_TongueTipBend': gains['tip_up']*source['tongueTip_up']+gains['tip_bend']*source['tongueTip_out'],
        'facs_css_TongueTipSide': gains['tip_side']*source['tongueTip_right'],
        'facs_css_TongueTipOut': gains['tip_out']*source['tongueTip_out'],
        'facs_css_TongueFlatten': source['tongue_flatten'],
        'facs_bs_EyeSquintLeft_div2': source['squint']+source['squint_left'],
        'facs_bs_EyeSquintRight_div2': source['squint'],
        'facs_ctrl_MouthSmile': source['lipCorner_pull'],
        'facs_bs_MouthShrugLower_div2': source['chin_up'],
        'facs_bs_MouthLowerDownLeft_div2': -source['lowerLip_up_left'],
    }
    for side in ('Left', 'Right'):
        row = eyelids[side]
        target['facs_jnt_EyeBlink'+side] = sum(
            (gain-row['native_squint_upper_lid_gain'])*source[name]
            for name, gain in row['source_upper_lid_gains'].items() if gain != 0)
    if not all(np.isfinite(value) and -.5 <= value <= 1.5 for value in target.values()):
        raise ValueError('Combined action exceeds the reviewed signed bake range')
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--eyelids', type=Path, required=True)
    parser.add_argument('--review', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--refine-report', type=Path, action='append', default=[])
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    protected = {}

    def protect(path):
        path = path.resolve(strict=True)
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        if protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Input changed: '+str(path))
        return path

    def read(path):
        return json.loads(protect(path).read_text())

    eyelids, review = read(args.eyelids), read(args.review)
    if not eyelids.get('measured') or not review.get('passed'):
        raise ValueError('Require measured eyelids and a reviewed combined mapping')
    for audit in (eyelids, review):
        for path, checksum in audit['protected'].items():
            if protected[str(protect(Path(path)))] != checksum:
                raise ValueError('Reviewed input changed: '+path)
    gains = review['gains']
    if (set(gains) != {'tongue_right', 'tongue_arch', 'tongue_out', 'tip_up', 'tip_bend', 'tip_side', 'tip_out'} or
            not all(np.isfinite(value) and -1 <= value <= 1 for value in gains.values())):
        raise ValueError('Unexpected reviewed control gains')
    document = read(args.catalog)
    face = next(f for f in document['catalog']['animation_library']['faces'] if f['id'] == 'css.eve.face')
    clip = next(c for c in face['performances'] if c['clip'] == 'css.eve.dance.comecloser')
    curves = {c['morph']: np.asarray(c['keys'], dtype=float) for c in clip['curves']}
    names = {'jaw_open', 'tongue_left', 'tongue_right', 'tongue_up', 'tongue_out', 'tongue_flatten',
             'tongue_arch', 'tongueTip_right', 'tongueTip_up', 'tongueTip_out', 'squint',
             'lipCorner_pull', 'chin_up', 'squint_left', 'lowerLip_up_left'}
    duration = float(clip['duration'])
    if (set(curves) != names or len(curves) != len(clip['curves']) or
            not np.isfinite(duration) or not 0 < duration <= 600):
        raise ValueError('Unexpected Come Closer source channels or duration')
    for name, keys in curves.items():
        if (keys.ndim != 2 or keys.shape[1] != 2 or len(keys) < 2 or not np.isfinite(keys).all() or
                np.any(np.diff(keys[:, 0]) <= 0) or keys[0, 0] < 0 or keys[-1, 0] > duration or
                keys[:, 1].min() < -.5 or keys[:, 1].max() > 1.5):
            raise ValueError('Invalid source keys: '+name)

    def pose(t, index, kind):
        source = {name: float(np.interp(t, keys[:, 0], keys[:, 1])) for name, keys in curves.items()}
        return dict(label=f'mix_{kind}_{index:05}', seconds=float(t), source_weights=source,
                    controls=mapped_controls(source, eyelids['calibration'], gains))

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
    for path in list(protected):
        protect(Path(path))
    args.output.mkdir(parents=True)
    for name, rows in [('combinations', samples), ('holdouts', holdouts)]:
        (args.output/(name+'.json')).write_text(json.dumps(
            [dict(label=r['label'], controls=r['controls']) for r in rows], indent=2, allow_nan=False)+'\n')
    report = dict(prepared=True, protected=protected, clip=clip['clip'], duration=duration,
                  samples=samples, holdouts=holdouts, source_key_times=source_times,
                  eye_gaze_range=False, tongue_fk=True, performance_mouth_controls=True,
                  reviewed_gains=gains, eyelid_calibration=eyelids['calibration'],
                  scope='Full source facial-action adaptation. Combined geometry, fitted export, cooking and live acceptance remain pending.')
    (args.output/'plan.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print('CW_COMECLOSER_PLAN', len(samples), len(holdouts), flush=True)


if __name__ == '__main__':
    main()

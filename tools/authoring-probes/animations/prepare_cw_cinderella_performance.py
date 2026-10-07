"""Map Cinderella's complete face track to measured eyelid and native CW actions."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--mapping', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--refine-report', type=Path, action='append', default=[])
    parser.add_argument('--control-step', type=float, default=.08)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    if not np.isfinite(args.control_step) or not .01 <= args.control_step <= .2:
        raise ValueError('Expected a bounded native-control sampling step')
    protected = {}

    def protect(path):
        path = path.resolve(strict=True)
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        if protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Protected input changed: '+str(path))
        return path

    def read(path):
        return json.loads(protect(path).read_text())

    mapping = read(args.mapping)
    if not mapping.get('prepared') or mapping['clip'] != 'css.eve.dance.dumbdumb':
        raise ValueError('Require the measured author-head eyelid mapping')
    for path, checksum in mapping['protected'].items():
        if protected[str(protect(Path(path)))] != checksum:
            raise ValueError('Mapped author-head or CW source changed')
    head_path = args.work/'author-head-source-v1/head-morphs-v2'
    head = read(head_path/'morph-audit.json')
    if head['vertex_count'] != 24296 or head['delta_stride'] != 28:
        raise ValueError('Author-head topology or morph encoding changed')
    blob = protect(head_path/'morph-deltas.bin').read_bytes()
    dtype = np.dtype([('index', '<u4'), ('position', '<f4', (3,)), ('normal', '<f4', (3,))])

    def delta(name):
        row = next(m for m in head['morphs'] if m['name'] == name)
        data = np.frombuffer(blob, dtype, row['count'], row['offset_bytes'])
        result = np.zeros((head['vertex_count'], 3))
        result[data['index']] = data['position']
        return result

    surprise_residual = float(np.linalg.norm(delta('surprise')-delta('upLid_up_L')-delta('upLid_up_R'), axis=1).max())
    if surprise_residual > 1e-7:
        raise ValueError('Surprise is no longer exactly bilateral eyelid opening')
    manifest = read(args.catalog)
    face = next(r for r in manifest['catalog']['animation_library']['faces'] if r['id'] == 'css.eve.face')
    clip = next(c for c in face['performances'] if c['clip'] == 'css.eve.dance.cinderella')
    curves = {row['morph']: np.asarray(row['keys'], dtype=float) for row in clip['curves']}
    direct = {'jaw_open': 'facs_jnt_JawOpen', 'smile': 'facs_ctrl_MouthSmile',
              'iii': 'facs_ctrl_vIY', 'woo': 'facs_ctrl_vUW', 'eee': 'facs_ctrl_vEE',
              'ooo': 'facs_ctrl_vOW', 'innerBrow_up': 'facs_ctrl_BrowInnerUp'}
    if (len(curves) != len(clip['curves']) or set(curves) != set(direct) |
            {'blink', 'lid_mid_closed_L', 'lid_mid_closed_R', 'upLid_down_L', 'surprise'}):
        raise ValueError('Unmapped Cinderella source channel')
    duration = float(clip['duration'])
    if not np.isfinite(duration) or not 0 < duration <= 600:
        raise ValueError('Invalid source duration')
    for name, keys in curves.items():
        if (keys.ndim != 2 or keys.shape[1] != 2 or len(keys) < 2 or not np.isfinite(keys).all() or
                np.any(np.diff(keys[:, 0]) <= 0) or keys[0, 0] < 0 or keys[-1, 0] > duration or
                keys[:, 1].min() < 0 or keys[:, 1].max() > 1):
            raise ValueError('Unexpected source keys: '+name)

    def controls(seconds):
        source = {name: float(np.interp(seconds, keys[:, 0], keys[:, 1])) for name, keys in curves.items()}
        target = {name: source[key] for key, name in direct.items()}
        for side, suffix in [('L', 'Left'), ('R', 'Right')]:
            target['facs_jnt_EyeBlink'+suffix] = source['blink']+sum(
                source[name]*row['gain'] for name, row in mapping['eyelid_calibration'][side]['actions'].items())
            target['facs_jnt_EyesWide'+suffix] = source['surprise']
        # The authored upper-lid action is a complete unilateral closure.
        # Use CW's own full wink, retaining her eyelid proportions.
        target['facs_jnt_EyeBlinkLeft'] += source['upLid_down_L']
        if not all(0 <= value <= 1.01 for value in target.values()):
            raise ValueError('Combined facial action exceeds the verified native range')
        return source, target

    source_times = sorted({float(key[0]) for keys in curves.values() for key in keys} | {0., duration})
    times = set(source_times)
    for a, b in zip(source_times, source_times[1:]):
        ca, cb = controls(a)[1], controls(b)[1]
        steps = max(1, int(np.ceil(max(abs(ca[k]-cb[k]) for k in ca)/args.control_step)))
        times.update(np.linspace(a, b, steps+1))
    times = sorted(times)
    for path in args.refine_report:
        prior = read(path)
        if prior['clip'] != clip['clip'] or len(prior['holdouts']) != len(times)-1:
            raise ValueError('Refinement report differs from the source plan')
        extra = []
        for a, b, row in zip(times[:-1], times[1:], prior['holdouts'], strict=True):
            if abs(row['seconds']-(a+b)/2) > 1e-12:
                raise ValueError('Refinement times differ')
            divisions = 8 if row['error_cm'] > .01 else 4 if row['error_cm'] > .004 else 1
            extra.extend(np.linspace(a, b, divisions+1)[1:-1])
        times = sorted(set(times) | set(extra))
    if len(times) > 16384:
        raise ValueError('Source plan exceeds the bounded sample count')

    def row(seconds, index, kind):
        source, target = controls(seconds)
        return dict(label=f'mix_{kind}_{index:05}', seconds=float(seconds), controls=target, source_weights=source)

    samples = [row(t, i, 'frame') for i, t in enumerate(times)]
    holdouts = [row((a+b)/2, i, 'holdout') for i, (a, b) in enumerate(zip(times, times[1:]))]
    for path in list(protected):
        protect(Path(path))
    args.output.mkdir(parents=True)
    for name, rows in [('combinations', samples), ('holdouts', holdouts)]:
        (args.output/(name+'.json')).write_text(json.dumps([dict(label=r['label'], controls=r['controls']) for r in rows], indent=2)+'\n')
    report = dict(prepared=True, protected=protected, clip=clip['clip'], duration=duration,
                  samples=samples, holdouts=holdouts, source_key_times=source_times,
                  eye_gaze_range=False, tongue_fk=False, direct_native_actions=direct, control_step=args.control_step,
                  eyelid_calibration=mapping['eyelid_calibration'], surprise_residual_cm=surprise_residual,
                  unilateral_closure='upLid_down_L maps to the native full left wink',
                  scope='Full source action mapping. Native combined geometry, fitted representation, cooked and live acceptance remain pending.')
    (args.output/'plan.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print('CW_CINDERELLA_PLAN', len(samples), len(holdouts), flush=True)


if __name__ == '__main__':
    main()

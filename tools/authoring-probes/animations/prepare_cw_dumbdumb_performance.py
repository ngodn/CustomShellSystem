"""Adapt DumbDumb to CW's native facial actions, retaining every source control and key time."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True)
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

    work = args.work.resolve(strict=True)
    head_path = work/'author-head-source-v1/head-morphs-v2'
    head = read(head_path/'morph-audit.json')
    for row in head['files']:
        if protected[str(protect(head_path/row['name']))] != row['sha256']:
            raise ValueError('Audited author head changed')
    vertices = np.fromfile(head_path/'morph-vertices.bin', '<f4').reshape(-1, 5)[:, [1, 0, 2]].astype(float)
    blob = (head_path/'morph-deltas.bin').read_bytes()
    dtype = np.dtype([('index', '<u4'), ('position', '<f4', (3,)), ('normal', '<f4', (3,))])

    def delta(name):
        row = next(m for m in head['morphs'] if m['name'] == name)
        values = np.frombuffer(blob, dtype, row['count'], row['offset_bytes'])
        result = np.zeros_like(vertices)
        result[values['index']] = values['position'][:, [1, 0, 2]]
        return result

    def section(index, material):
        row = head['sections'][index]
        if material not in head['materials'][row['MaterialIndex']]['name']:
            raise ValueError('Author head section differs')
        return np.arange(row['BaseVertexIndex'], row['BaseVertexIndex']+row['NumVertices'])

    blink = delta('blink')
    skin = section(1, 'Head_V02_Jali')
    eyelids = skin[np.linalg.norm(blink[skin], axis=1) > .15]
    calibration = {}
    for side, sign in [('L', 1), ('R', -1)]:
        selected = eyelids[vertices[eyelids, 0]*sign > 0]
        reference = blink[selected]
        if len(selected) < 150:
            raise ValueError('Insufficient measured eyelid points')
        values = {}
        for name in ('smile', 'lid_mid_closed_'+side):
            moved = delta(name)[selected]
            gain = float(np.sum(moved*reference)/np.sum(reference**2))
            if not .4 < gain < .9:
                raise ValueError('Author eyelid action changed')
            values[name] = dict(gain=gain, residual_cm=float(np.linalg.norm(moved-reference*gain, axis=1).max()))
        calibration[side] = dict(indices=selected.tolist(), actions=values)
    brows = section(6, 'EyeBrow')
    furrow, minor = delta('brow_furrow')[brows], delta('brow_down_miner')[brows]
    brow_gain = float(np.sum(minor*furrow)/np.sum(furrow**2))
    if not .2 < brow_gain < .7:
        raise ValueError('Author brow action changed')
    manifest = read(args.catalog)
    face = next(r for r in manifest['catalog']['animation_library']['faces'] if r['id'] == 'css.eve.face')
    clip = next(c for c in face['performances'] if c['clip'] == 'css.eve.dance.dumbdumb')
    curves = {row['morph']: np.asarray(row['keys'], dtype=float) for row in clip['curves']}
    simple = {'jaw_open': 'facs_jnt_JawOpen', 'lipCorner_down': 'facs_ctrl_MouthFrown',
              'smile': 'facs_ctrl_MouthSmile', 'iii': 'facs_ctrl_vIY', 'woo': 'facs_ctrl_vUW',
              'eee': 'facs_ctrl_vEE', 'ooo': 'facs_ctrl_vOW'}
    if (len(curves) != len(clip['curves']) or
            set(curves) != set(simple) | {'brow_furrow', 'brow_down_miner', 'blink', 'lid_mid_closed_L', 'lid_mid_closed_R'}):
        raise ValueError('Unmapped source curves')
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
        target = {name: source[key] for key, name in simple.items()}
        target['facs_ctrl_BrowDown'] = source['brow_furrow']+brow_gain*source['brow_down_miner']
        for side, suffix in [('L', 'Left'), ('R', 'Right')]:
            target['facs_jnt_EyeBlink'+suffix] = source['blink']+sum(
                source[name]*row['gain'] for name, row in calibration[side]['actions'].items())
        if not all(0 <= value <= 1.01 for value in target.values()):
            raise ValueError('Combined mapping needs further source-range calibration')
        return source, target

    source_times = sorted({float(key[0]) for keys in curves.values() for key in keys} | {0., duration})
    times = set(source_times)
    for a, b in zip(source_times, source_times[1:]):
        ca, cb = controls(a)[1], controls(b)[1]
        steps = max(1, int(np.ceil(max(abs(ca[k]-cb[k]) for k in ca)/.08)))
        times.update(np.linspace(a, b, steps+1))
    times = sorted(times)
    for report_path in args.refine_report:
        previous = read(report_path)
        if previous['clip'] != clip['clip'] or len(previous['holdouts']) != len(times)-1:
            raise ValueError('Refinement report does not match the source plan')
        extra = []
        for a, b, case in zip(times[:-1], times[1:], previous['holdouts'], strict=True):
            if abs(case['seconds']-(a+b)/2) > 1e-12:
                raise ValueError('Refinement interval differs')
            divisions = 8 if case['error_cm'] > .01 else 4 if case['error_cm'] > .004 else 1
            extra.extend(np.linspace(a, b, divisions+1)[1:-1])
        times = sorted(set(times) | set(extra))
    if len(times) > 8192:
        raise ValueError('Mapping exceeds the bounded source sample count')

    def row(seconds, index, kind):
        source, target = controls(seconds)
        return dict(label=f'mix_{kind}_{index:05}', seconds=float(seconds), controls=target, source_weights=source)

    samples = [row(t, i, 'frame') for i, t in enumerate(times)]
    holdouts = [row((a+b)/2, i, 'holdout') for i, (a, b) in enumerate(zip(times, times[1:]))]
    for folder in ('cw-performance-controls-v1', 'cw-facial-samples-v1'):
        path = work/folder/'samples/report.json' if (work/folder/'samples/report.json').exists() else work/folder/'report.json'
        report = read(path)
        if not report['sources_unchanged']:
            raise ValueError('Native CW control calibration is incomplete')
        if protected[str(protect(Path(report['source'])))] != report['sha256']:
            raise ValueError('CW source changed')
    for path in list(protected):
        protect(Path(path))
    args.output.mkdir(parents=True)
    for filename, rows in [('combinations.json', samples), ('holdouts.json', holdouts)]:
        (args.output/filename).write_text(json.dumps([dict(label=r['label'], controls=r['controls']) for r in rows], indent=2)+'\n')
    report = dict(prepared=True, protected=protected, clip=clip['clip'], duration=duration,
                  source_key_times=source_times, samples=samples, holdouts=holdouts,
                  eye_gaze_range=False, tongue_fk=False, direct_native_actions=simple,
                  eyelid_calibration=calibration, brow_minor_gain=brow_gain,
                  brow_minor_residual_cm=float(np.linalg.norm(minor-brow_gain*furrow, axis=1).max()),
                  scope='Character-specific action adaptation, not geometric identity across faces. Smile retains measured eyelid closure; the weaker brow action retains measured strength. CW keeps her own mouth and phoneme proportions. Combined geometry and visual acceptance remain pending.')
    (args.output/'plan.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print('CW_DUMBDUMB_PLAN', len(samples), len(holdouts), flush=True)


if __name__ == '__main__':
    main()

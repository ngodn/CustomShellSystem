"""Evaluate a complete prepared CW performance in bounded, independently restored Blender batches."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

import numpy as np


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blender', type=Path, required=True)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--kind', choices=('samples', 'holdouts'), required=True)
    parser.add_argument('--reuse', type=Path, action='append', default=[],
                        help='Reuse identical completed poses from the same source, sampler and bake settings')
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    plan_path, source = args.plan.resolve(strict=True), args.source.resolve(strict=True)
    protected = {str(path): digest(path) for path in (plan_path, source, Path(__file__).with_name('sample_cw_facial_controls.py'))}
    plan = json.loads(plan_path.read_text())
    if plan.get('performance_mouth_controls'):
        helper = Path(__file__).with_name('cw_performance_controls.py').resolve(strict=True)
        protected[str(helper)] = digest(helper)
    if not plan.get('prepared') or plan['protected'].get(str(source)) != protected[str(source)]:
        raise ValueError('Plan does not identify the verified source')
    rows = plan[args.kind]
    if not 1 <= len(rows) <= 16384 or len({row['label'] for row in rows}) != len(rows):
        raise ValueError('Invalid bounded sample list')
    controls = sorted({name for row in rows for name in row['controls']})
    output = args.output.resolve()
    samples = output/'samples'
    samples.mkdir(parents=True)
    (output/'plan.json').write_bytes(plan_path.read_bytes())
    started = time.monotonic()
    receipts = []
    neutral = None
    original_neutral = None
    available = {}

    def retain_original(path):
        nonlocal original_neutral
        path = path.resolve(strict=True)
        checksum = digest(path)
        if protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Original-mode neutral changed during sampling')
        with np.load(path) as data:
            current = {name: data[name].copy() for name in data.files}
        if original_neutral is None:
            original_neutral = current
            (samples/'original-neutral.npz').hardlink_to(path)
        elif (set(original_neutral) != set(current) or
              any(not np.array_equal(original_neutral[name], current[name]) for name in original_neutral)):
            raise ValueError('Original-mode neutral differs across source batches')

    def identity(row):
        return json.dumps([row['seconds'], row['controls']], sort_keys=True, separators=(',', ':'), allow_nan=False)

    for folder in args.reuse:
        folder = folder.resolve(strict=True)
        report_path = folder/'samples/report.json'
        prior_plan_path = folder/'plan.json'
        prior = json.loads(report_path.read_text())
        prior_plan = json.loads(prior_plan_path.read_text())
        if (json.loads((folder/'exit.json').read_text())['exit_code'] != 0 or
                not prior.get('sources_unchanged') or prior['sha256'] != protected[str(source)] or
                prior_plan['clip'] != plan['clip'] or
                prior['eye_gaze_range'] != plan.get('eye_gaze_range', False) or
                prior['tongue_fk'] != plan.get('tongue_fk', False) or
                prior.get('signed_mouth_range', False) != plan.get('signed_mouth_range', False) or
                prior.get('performance_mouth_controls', False) != plan.get('performance_mouth_controls', False) or
                (plan.get('performance_mouth_controls') and prior['protected'].get(str(helper)) != protected[str(helper)]) or
                prior['protected'].get(str(Path(__file__).with_name('sample_cw_facial_controls.py'))) !=
                protected[str(Path(__file__).with_name('sample_cw_facial_controls.py'))]):
            raise ValueError('Reuse requires matching verified source and sampler settings')
        for path in (report_path, prior_plan_path):
            protected[str(path)] = digest(path)
        if plan.get('tongue_fk'):
            if prior.get('original_neutral_reference'):
                path = Path(prior['original_neutral_reference'])
                if digest(path) != prior['original_neutral_sha256']:
                    raise ValueError('Original-mode neutral changed')
            else:
                if not prior.get('batches'):
                    raise ValueError('Reuse has no verified original-mode neutral')
                first = prior['batches'][0]
                report = Path(first['path'])
                if digest(report) != first['sha256']:
                    raise ValueError('Original-mode source batch changed')
                protected[str(report)] = first['sha256']
                path = report.parent/'original-neutral.npz'
            retain_original(path)
        for row in prior_plan['samples']+prior_plan['holdouts']:
            file = folder/'samples'/(row['label']+'.npz')
            if file.exists():
                available.setdefault(identity(row), file)
        with np.load(folder/'samples/neutral.npz') as data:
            current = {name: data[name].copy() for name in data.files}
        if neutral is None:
            neutral = current
            (samples/'neutral.npz').hardlink_to(folder/'samples/neutral.npz')
        elif set(neutral) != set(current) or any(not np.array_equal(neutral[name], current[name]) for name in neutral):
            raise ValueError('Reuse neutral geometry differs')
    pending, reused = [], []
    for row in rows:
        previous = available.get(identity(row))
        if previous is None:
            pending.append(row)
        else:
            protected[str(previous)] = digest(previous)
            (samples/(row['label']+'.npz')).hardlink_to(previous)
            reused.append(dict(label=row['label'], source=str(previous), sha256=protected[str(previous)]))
    print('CW_BATCH_PLAN', args.kind, 'reused', len(reused), 'new', len(pending), flush=True)
    for index, offset in enumerate(range(0, len(pending), 512)):
        batch = output/f'batch-{index:02}'
        batch.mkdir()
        selected = pending[offset:offset+512]
        combinations = [dict(label=row['label'], controls=row['controls']) for row in selected]
        file = batch/'combinations.json'
        file.write_text(json.dumps(combinations, separators=(',', ':'), allow_nan=False)+'\n')
        command = [str(args.blender.resolve(strict=True)), '--background', '--factory-startup', '--disable-autoexec',
                   '--threads', '2', '--python-exit-code', '1', '--python',
                   str(Path(__file__).with_name('sample_cw_facial_controls.py').resolve()), '--',
                   '--source', str(source), '--output', str(batch/'samples'), '--levels', '.5',
                   '--extended-levels', '--omit-bone-records', '--combinations', str(file)]
        if plan.get('eye_gaze_range'):
            command.append('--eye-gaze-range')
        if plan.get('tongue_fk'):
            command.append('--tongue-fk')
        if plan.get('signed_mouth_range'):
            command.append('--signed-mouth-range')
        if plan.get('performance_mouth_controls'):
            command.append('--performance-mouth-controls')
        for control in controls:
            command += ['--control', control]
        (batch/'command.json').write_text(json.dumps(command, indent=2)+'\n')
        before = time.monotonic()
        with (batch/'output.log').open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        (batch/'exit.json').write_text(json.dumps(dict(exit_code=result.returncode, seconds=time.monotonic()-before))+'\n')
        if result.returncode:
            raise RuntimeError('Source batch failed: '+str(batch))
        report_path = batch/'samples/report.json'
        report = json.loads(report_path.read_text())
        if (not report['sources_unchanged'] or report['sha256'] != protected[str(source)] or
                report['combination_plan_sha256'] != digest(file) or
                report['eye_gaze_range'] != plan.get('eye_gaze_range', False) or
                report['tongue_fk'] != plan.get('tongue_fk', False) or
                report.get('signed_mouth_range', False) != plan.get('signed_mouth_range', False) or
                report.get('performance_mouth_controls', False) != plan.get('performance_mouth_controls', False)):
            raise ValueError('Source batch settings or inputs changed')
        records = {row['label']: row for row in report['records'] if row['label'].startswith('mix_')}
        if set(records) != {row['label'] for row in selected}:
            raise ValueError('Source batch did not evaluate every requested pose')
        for row in selected:
            if any(abs(records[row['label']]['controls'][name]-value) > 1e-7 for name, value in row['controls'].items()):
                raise ValueError('Source control did not retain its requested value')
            (samples/(row['label']+'.npz')).hardlink_to(batch/'samples'/(row['label']+'.npz'))
        with np.load(batch/'samples/neutral.npz') as data:
            current = {name: data[name].copy() for name in data.files}
        if plan.get('tongue_fk'):
            retain_original(batch/'samples/original-neutral.npz')
        if neutral is None:
            neutral = current
            (samples/'neutral.npz').hardlink_to(batch/'samples/neutral.npz')
        elif set(neutral) != set(current) or any(not np.array_equal(neutral[name], current[name]) for name in neutral):
            raise ValueError('Neutral source geometry changed across batches')
        receipts.append(dict(path=str(report_path), sha256=digest(report_path), poses=len(selected)))
        print('CW_BATCH', args.kind, index+1, min(offset+512, len(pending)), '/', len(pending), flush=True)
    if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
        raise ValueError('Protected input changed during sampling')
    report = dict(source=str(source), sha256=protected[str(source)], sources_unchanged=True,
                  eye_gaze_range=plan.get('eye_gaze_range', False), tongue_fk=plan.get('tongue_fk', False),
                  signed_mouth_range=plan.get('signed_mouth_range', False),
                  performance_mouth_controls=plan.get('performance_mouth_controls', False),
                  batches=receipts, protected=protected, poses=len(rows), omitted_bone_records=True,
                  reused=reused, newly_evaluated=len(pending),
                  original_neutral_reference=str(samples/'original-neutral.npz') if original_neutral is not None else None,
                  original_neutral_sha256=digest(samples/'original-neutral.npz') if original_neutral is not None else None,
                  scope='Complete requested source poses with neutral/restoration checks in every bounded batch.')
    (samples/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    (output/'exit.json').write_text(json.dumps(dict(exit_code=0, seconds=time.monotonic()-started), indent=2)+'\n')


if __name__ == '__main__':
    main()

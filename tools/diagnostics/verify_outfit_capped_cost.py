"""Verify the outfit timing matrix, native-cap restoration and rig observations offline."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import sys

from check_outfit_switch_lifecycle import OUTFITS
from check_sidearm_aim_visibility import geometry_restored


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--prior', type=Path, action='append', default=[],
                        help='Retain verified completed cases from a fully restored earlier sequence')
    args = parser.parse_args()
    root = args.directory.resolve()
    matrix = read(root / 'result.json')
    assert matrix.get('only') is None
    assert matrix.get('requested_caps', [30., 60., 999.]) == [30., 60., 999.]
    assert matrix['passed'] and matrix['cases_passed']
    assert not matrix.get('error') and not matrix.get('restore_error')
    sources = [root, *[p.resolve() for p in args.prior]]
    assert len(sources) == len(set(sources))
    completed, failures, hashes = [], [], {}
    for source in sources:
        run = read(source / 'result.json')
        assert not run.get('restore_error') and all(run[k] for k in
            ('state_restored', 'geometry_restored', 'cine_restored', 'process_preserved'))
        assert read(source / 'state-before.json') == read(source / 'state-after.json') == read(root / 'state-before.json')
        assert read(source / 'cine-before.json') == read(source / 'cine-after.json') == read(root / 'cine-before.json')
        assert geometry_restored(run['geometry_before'], run['geometry_after'])
        assert geometry_restored(run['geometry_before'], matrix['geometry_before'])
        assert run['core'] == matrix['core'] and run['core_sha256'] == matrix['core_sha256']
        assert run['player'] == matrix['player'] and run.get('only') is None
        assert run.get('requested_caps', [30., 60., 999.]) == [30., 60., 999.]
        for case in run['cases']:
            assert 'returncode' in case, 'Earlier sequence is not terminal'
            if case.get('passed') and case['returncode'] == 0:
                completed.append((case, source))
            else:
                failures.append(dict(source=str(source), case=case, error=run.get('error')))
        if not run['passed']:
            assert run.get('error') and any(not c.get('passed') for c in run['cases'])
        hashes[str(source)] = {name: hashlib.sha256((source / name).read_bytes()).hexdigest()
                              for name in ('result.json', 'state-before.json', 'state-after.json',
                                           'cine-before.json', 'cine-after.json', 'recipe.py')}
    expected = {(o, v) for o, variants in OUTFITS.items() for v in variants}
    actual = [(r['outfit'], r['variant']) for r, source in completed]
    assert len(actual) == len(expected) and set(actual) == expected
    summaries = []
    frames = 0
    controls = set()
    for case, source in completed:
        assert case['passed'] and case['returncode'] == 0
        folder = Path(case['evidence'])
        assert folder.parent == source
        report = read(folder / 'result.json')
        assert report.get('requested_caps', [30., 60., 999.]) == [30., 60., 999.]
        assert report['rig_observer'] == 'secondary_rigs'
        assert (report['original']['outfit'], report['original']['variant']) == (case['outfit'], case['variant'])
        assert report['original']['shell'] == matrix['original']['shell']
        assert report['original']['idle_override'] == 'eve'
        assert geometry_restored(matrix['geometry_before'], report['geometry_before'])
        command = [sys.executable, str(Path(__file__).with_name('verify_capped_cost.py')), str(folder)]
        run = subprocess.run(command, check=True, capture_output=True, text=True)
        verified = read(folder / 'verified.json')
        assert json.loads(run.stdout)['passed'] and verified['passed']
        controls.update(verified['rejected_controls'])
        frames += verified['frames']
        for cap_case in report['cases']:
            cap = cap_case['cap']
            capture = read(folder / ('cap-' + str(int(cap))) / 'result.json')
            assert capture['core_sha256'] == matrix['core_sha256']
            assert capture['core'] == matrix['core']
            assert capture['context_before']['player'] == matrix['player']
            selected = [row for row in verified['summaries'] if row['cap'] == cap]
            assert len(selected) == 3
            timings = {
                metric: {q: statistics.median(row['metrics'][metric][q] for row in selected)
                         for q in ('p50', 'p95', 'p99')}
                for metric in ('interval_ms', 'engine_ms', 'core_ms')}
            counters = []
            for old, new in zip(cap_case['rig_before']['rigs'], cap_case['rig_after']['rigs']):
                if 'TotalSteps' in old['values']:
                    counters.append(dict(class_path=old['object']['class'], enabled=old['values'].get('Enabled'),
                                         steps=new['values']['TotalSteps'] - old['values']['TotalSteps'],
                                         reset_count=new['values'].get('ResetCount')))
            summaries.append(dict(outfit=case['outfit'], variant=case['variant'], cap=cap,
                                  observed_hz=[row['engine_tick_rate_hz'] for row in selected],
                                  median_run_statistics=timings, rig_counters=counters))
    assert controls == {'changed priority', 'stalled rig', 'nonfinite rig'}
    result = dict(passed=True, outfits=len(completed), captures=len(summaries) * 3,
                  frames=frames, rejected_controls=sorted(controls), summaries=summaries,
                  hashes=hashes, prior_failed_attempts=failures, all_attempts_passed=not failures,
                  scope='Stationary timing and boundary rig reads at native caps; no matched baseline, GPU or cloth-cost claim.')
    target = root / ('combined-verified.json' if args.prior else 'verified.json')
    target.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('summaries', 'hashes', 'prior_failed_attempts')}))


if __name__ == '__main__':
    main()

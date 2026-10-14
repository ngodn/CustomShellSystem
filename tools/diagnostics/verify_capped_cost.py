"""Verify archived frame-cap captures without contacting the game."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import statistics


def read(path):
    return json.loads(path.read_text())


def finite(value):
    if isinstance(value, dict):
        return all(finite(v) for v in value.values())
    if isinstance(value, list):
        return all(finite(v) for v in value)
    return not isinstance(value, float) or math.isfinite(value)


def verify_capture(capture):
    rows, summary = capture['raw']['rows'], capture['summary']
    assert capture['state_unchanged'] and capture['raw']['stop_reason'] == 'duration'
    assert len(rows) == summary['frames'] and len(rows) >= 30
    assert all(not row['failed'] for row in rows)
    phases = capture['raw']['phases']
    assert all(len(row['phase_ms']) == len(phases) for row in rows)
    assert all(sum(row['phase_ms']) <= row['core_ms'] + .05 for row in rows)
    columns = {name: [row[name] for row in rows] for name in ('interval_ms', 'engine_ms', 'core_ms')}
    columns.update({name + '_ms': [row['phase_ms'][i] for row in rows] for i, name in enumerate(phases)})
    for name, values in columns.items():
        assert all(math.isfinite(v) and v >= 0 for v in values)
        ordered = sorted(values)
        expected = dict(mean=statistics.fmean(values), p50=statistics.median(values),
                        p95=ordered[math.ceil(len(values) * .95) - 1],
                        p99=ordered[math.ceil(len(values) * .99) - 1], max=max(values))
        assert expected == summary['metrics'][name], name
    assert min(columns['interval_ms']) > 0
    assert math.isclose(summary['engine_tick_rate_hz'], 1000 / statistics.fmean(columns['interval_ms']))


def verify_case(case, flags):
    assert case['passed'] and case['returncode'] == 0
    assert case['before'] == case['after'] == flags
    before, after = case['rig_before'], case['rig_after']
    assert finite(before) and finite(after)
    assert before['public'] == after['public']
    for key in before['public']:
        assert before['rig'][key] == after['rig'][key] == before['public'][key]
    assert after['rig']['TotalSteps'] > before['rig']['TotalSteps']
    assert after['rig']['SeenUpdate'] > before['rig']['SeenUpdate']
    assert after['rig']['ResetCount'] == before['rig']['ResetCount']
    for key in ('Limits', 'MaxTravel'):
        assert before['rig'][key] == after['rig'][key]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    root = parser.parse_args().directory.resolve()
    report = read(root / 'result.json')
    assert report['passed'] and report['captures_complete']
    assert not report.get('error') and not report.get('restore_error')
    assert all(report[k] for k in ('state_restored', 'cine_restored', 'settings_file_unchanged',
        'cap_restored', 'flags_restored', 'graphics_restored', 'geometry_restored', 'focus_restored'))
    assert report['flags_before'] == report['flags_after']
    assert report['flags_before']['set_by'] != 0
    assert report['cvars_before'] == report['cvars_after']
    assert report['geometry_before'] == report['geometry_after']
    assert hashlib.sha256((root / 'GameUserSettings-before.ini').read_bytes()).hexdigest() == report['settings_sha256']
    assert [r['cap'] for r in report['cases']] == [30., 60., report['original_cap']]
    requests = [read_row['request'] for line in (root / 'requests.jsonl').read_text().splitlines()
                if 'request' in (read_row := json.loads(line))]
    calls = [r for r in requests if r['op'] == 'call']
    assert {r['function'] for r in calls} <= {'GetSpartaUserSettings', 'GetFrameRateLimit_Always',
        'SetFrameRateLimit_Always', 'K2_GetActorLocation', 'K2_GetActorRotation',
        'GetControlRotation', 'GetViewTarget', 'GetConsoleVariableStringValue'}
    assert [r['args']['NewLimitFPS'] for r in calls if r['function'] == 'SetFrameRateLimit_Always'] == [30., 60., 999., 999.]
    summaries = []
    for case in report['cases']:
        verify_case(case, report['flags_before'])
        folder = root / ('cap-' + str(int(case['cap'])))
        run = read(folder / 'result.json')
        assert case['capture'] == run and run['passed'] and len(run['samples']) == 3
        assert run['process_before'] == run['process_after'] == [report['flags_before']['pid']]
        assert run['saved_state_equal'] and run['core_unchanged']
        assert read(folder / 'state-before.json') == read(root / 'state-before.json')
        assert float(run['context_before']['cvars']['t.MaxFPS']) == case['cap']
        for i, sample in enumerate(run['samples'], 1):
            capture = read(folder / f'capture-{i}.json')
            verify_capture(capture)
            assert sample['returncode'] == 0 and sample['context_after'] == run['context_before']
            assert capture['summary'] == sample['summary']
            assert capture['core_sha256'] == run['core_sha256'] and capture['pids'] == run['process_before']
            summaries.append(dict(cap=case['cap'], capture=i, **capture['summary']))
    rejected = []
    for label in ('changed priority', 'stalled rig', 'nonfinite rig'):
        case = copy.deepcopy(report['cases'][0])
        if label == 'changed priority':
            case['after']['flags'] ^= 0x01000000
        elif label == 'stalled rig':
            case['rig_after']['rig']['TotalSteps'] = case['rig_before']['rig']['TotalSteps']
        else:
            case['rig_after']['rig']['StepSeconds'] = float('nan')
        try:
            verify_case(case, report['flags_before'])
        except AssertionError:
            rejected.append(label)
    assert len(rejected) == 3
    result = dict(passed=True, captures=len(summaries), frames=sum(s['frames'] for s in summaries),
                  rejected_controls=rejected, summaries=summaries,
                  scope='Archive verification of capped stationary Peony, not a baseline comparison or GPU measurement.')
    (root / 'verified.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'summaries'}))


if __name__ == '__main__':
    main()

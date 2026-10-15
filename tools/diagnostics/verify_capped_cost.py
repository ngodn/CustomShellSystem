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


def verify_secondary_rigs(before, after, *, record_resets=False):
    assert finite(before) and finite(after)
    assert before['post'] == after['post'] and before['component'] == after['component']
    assert len(before['rigs']) == len(after['rigs'])
    measured = 0
    resets = []
    for old, new in zip(before['rigs'], after['rigs']):
        assert old['node'] == new['node'] and old['object'] == new['object']
        a, b = old['values'], new['values']
        assert a.keys() == b.keys()
        for key in ('Enabled', 'Limits', 'MaxTravel'):
            if key in a:
                assert a[key] == b[key], key
        if 'ResetCount' in a:
            assert isinstance(a['ResetCount'], int) and isinstance(b['ResetCount'], int)
            delta = b['ResetCount'] - a['ResetCount']
            assert delta >= 0, 'ResetCount decreased'
            if delta:
                assert record_resets, 'ResetCount'
                resets.append(dict(node=old['node'], object=old['object'],
                                   before=a['ResetCount'], after=b['ResetCount'], delta=delta))
        if 'TotalSteps' in a and a.get('Enabled', True):
            assert b['TotalSteps'] > a['TotalSteps'], old['object']['class']
            measured += 1
        if 'SeenUpdate' in a and a.get('Enabled', True):
            assert b['SeenUpdate'] > a['SeenUpdate']
    assert measured > 0
    return resets


def verify_case(case, flags, observer='eve_body', *, record_resets=False):
    assert case['returncode'] == 0
    assert case['before'] == case['after'] == flags
    before, after = case['rig_before'], case['rig_after']
    if observer == 'secondary_rigs':
        resets = verify_secondary_rigs(before, after, record_resets=record_resets)
        if record_resets:
            assert case['observations_complete'] and case['reset_events'] == resets
            assert case['passed'] == (not resets)
        else:
            assert case['passed']
        return resets
    assert case['passed'] and not record_resets
    assert observer == 'eve_body'
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
    parser.add_argument('--check-only', action='store_true')
    parser.add_argument('--observations', action='store_true',
                        help='Verify complete data and restoration while retaining reset events as unresolved')
    args = parser.parse_args()
    root = args.directory.resolve()
    report = read(root / 'result.json')
    assert report['captures_complete']
    if args.observations:
        assert report['record_resets'] and report['observations_complete']
        assert report['rig_observer'] == 'secondary_rigs'
    else:
        assert report['passed']
    assert not report.get('error') and not report.get('restore_error')
    assert all(report[k] for k in ('state_restored', 'cine_restored', 'settings_file_unchanged',
        'cap_restored', 'flags_restored', 'graphics_restored', 'geometry_restored', 'focus_restored'))
    assert report['flags_before'] == report['flags_after']
    assert report['flags_before']['set_by'] != 0
    assert report['cvars_before'] == report['cvars_after']
    assert report['geometry_before'] == report['geometry_after']
    assert hashlib.sha256((root / 'GameUserSettings-before.ini').read_bytes()).hexdigest() == report['settings_sha256']
    expected_caps = report.get('requested_caps', [30., 60., report['original_cap']])
    assert expected_caps and len(expected_caps) == len(set(expected_caps))
    assert set(expected_caps) <= {30., 60., report['original_cap']}
    assert [r['cap'] for r in report['cases']] == expected_caps
    requests = [read_row['request'] for line in (root / 'requests.jsonl').read_text().splitlines()
                if 'request' in (read_row := json.loads(line))]
    calls = [r for r in requests if r['op'] == 'call']
    assert {r['function'] for r in calls} <= {'GetSpartaUserSettings', 'GetFrameRateLimit_Always',
        'SetFrameRateLimit_Always', 'K2_GetActorLocation', 'K2_GetActorRotation',
        'GetControlRotation', 'GetViewTarget', 'GetConsoleVariableStringValue'}
    assert [r['args']['NewLimitFPS'] for r in calls if r['function'] == 'SetFrameRateLimit_Always'] == expected_caps + [report['original_cap']]
    summaries, resets = [], []
    for case in report['cases']:
        events = verify_case(case, report['flags_before'], report.get('rig_observer', 'eve_body'),
                             record_resets=args.observations)
        resets.extend(dict(cap=case['cap'], **event) for event in (events or []))
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
        elif report.get('rig_observer') == 'secondary_rigs':
            old = next(r for r in case['rig_before']['rigs']
                       if 'TotalSteps' in r['values'] and r['values'].get('Enabled', True))
            new = next(r for r in case['rig_after']['rigs'] if r['node'] == old['node'])
            if label == 'stalled rig':
                new['values']['TotalSteps'] = old['values']['TotalSteps']
            else:
                new['values']['TotalSteps'] = float('nan')
        elif label == 'stalled rig':
            case['rig_after']['rig']['TotalSteps'] = case['rig_before']['rig']['TotalSteps']
        else:
            case['rig_after']['rig']['StepSeconds'] = float('nan')
        try:
            verify_case(case, report['flags_before'], report.get('rig_observer', 'eve_body'),
                        record_resets=args.observations)
        except AssertionError:
            rejected.append(label)
    assert len(rejected) == 3
    if args.observations:
        assert report['passed'] == (not resets)
    result = dict(passed=not resets, captures=len(summaries), frames=sum(s['frames'] for s in summaries),
                  rejected_controls=rejected, summaries=summaries,
                  scope='Archive verification of capped stationary outfit, not a baseline comparison or GPU measurement.')
    if args.observations:
        result.update(observation_integrity_verified=True, reset_events=resets,
                      reset_free=not resets)
    if not args.check_only:
        name = 'observations-verified.json' if args.observations else 'verified.json'
        (root / name).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'summaries'}))


if __name__ == '__main__':
    main()

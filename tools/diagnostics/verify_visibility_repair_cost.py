"""Review matched visibility-repair episodes without contacting the game."""
import argparse
import json
from pathlib import Path
import statistics

from verify_genessa_package_lineage import sha


def metric(values):
    values = sorted(values)
    assert values and all(0 <= v < float('inf') for v in values)
    import math
    return {key: value for key, value in (
        ('p50', statistics.median(values)), ('p95', values[math.ceil(.95 * len(values)) - 1]),
        ('p99', values[math.ceil(.99 * len(values)) - 1]), ('max', max(values)),
        ('mean', statistics.fmean(values)))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    args = parser.parse_args()
    folder = args.capture.resolve()
    out = folder / 'independent-verification.json'
    assert not out.exists()
    report = json.loads((folder / 'result.json').read_text())
    assert report['passed'] and report['captures_complete']
    assert len(report['captures']) == 9 and len(report['reloads']) == 2
    assert all(row['verified'] for row in report['reloads'])
    checks = ('state_restored', 'selector_restored', 'flags_restored', 'geometry_restored', 'resources_restored',
              'graphics_restored', 'cine_restored', 'keys_released', 'process_unchanged', 'focus_restored')
    assert all(report[k] is True for k in checks)
    blocks = {}
    total_events = total_frames = 0
    inputs = {str(folder / 'result.json'): sha(folder / 'result.json'),
              str(folder / 'recipe.py'): sha(folder / 'recipe.py')}
    expected_phases = ['recovery', 'inventory', 'maintenance', 'attachments', 'seals', 'walk', 'misc', 'reconcile', 'astral']
    for index, capture in enumerate(report['captures']):
        block = index // 3 + 1
        label = 'baseline' if block == 2 else 'candidate'
        assert capture['name'] == f'{block}-{label}-{index % 3 + 1}'
        assert capture['core'] == report['cores'][label][0]
        path = folder / capture['raw_file']
        assert path.parent == folder and sha(path) == capture['raw_sha256']
        inputs[str(path)] = capture['raw_sha256']
        raw = json.loads(path.read_text())
        assert raw['id'] == capture['profile_id'] and raw['stop_reason'] == 'duration'
        assert raw['phases'] == expected_phases
        assert len(raw['rows']) >= 250
        events = capture['events']
        assert len(events) == report['events_per_capture'] == 18
        assert all(row['hidden_after'] is True and row['ack_ns'] >= row['sent_ns'] for row in events)
        assert all(b['sent_ns'] > a['ack_ns'] for a, b in zip(events, events[1:]))
        assert events[0]['sent_ns'] - capture['arm_ack_ns'] >= 1_000_000_000
        assert events[-1]['ack_ns'] - capture['arm_sent_ns'] < 10_000_000_000
        if label == 'candidate':
            assert capture['flags_before'] == capture['flags_after'] == report['flags_before']
        values = blocks.setdefault(str(block), {'label': label, 'rows': [], 'episodes': []})
        for row in raw['rows']:
            assert row['failed'] is False and len(row['phase_ms']) == 9 and row['interval_ms'] > 0
            assert all(0 <= v < float('inf') for v in row['phase_ms'])
            assert sum(row['phase_ms']) <= row['core_ms'] + .05
        values['rows'].extend(raw['rows'])
        values['episodes'].append(dict(name=capture['name'], frames=len(raw['rows']),
            misc_ms=metric([r['phase_ms'][6] for r in raw['rows']]),
            core_ms=metric([r['core_ms'] for r in raw['rows']]),
            interval_ms=metric([r['interval_ms'] for r in raw['rows']])))
        total_events += len(events)
        total_frames += len(raw['rows'])
    requests_path = folder / 'requests.jsonl'
    inputs[str(requests_path)] = sha(requests_path)
    calls = [json.loads(line)['request'] for line in requests_path.read_text().splitlines()
             if 'request' in json.loads(line)]
    drift = [c for c in calls if c.get('op') == 'call' and c.get('function') == 'SetActorHiddenInGame']
    assert len(drift) == total_events == 162
    assert all(c['args'] == {'bNewHidden': False} and 'WP_Trebuchaxe_C' in c['target']['class'] for c in drift)
    for value in blocks.values():
        rows = value.pop('rows')
        value.update(frames=len(rows), misc_ms=metric([r['phase_ms'][6] for r in rows]),
                     core_ms=metric([r['core_ms'] for r in rows]), interval_ms=metric([r['interval_ms'] for r in rows]))
    result = dict(passed=True, frames=total_frames, events=total_events, blocks=blocks, inputs=inputs,
                  scope='Episode-level observations, not isolated per-event cost or causal FPS attribution. '
                        'No GPU, cloth-worker or display presentation timing. The sequence is candidate/baseline/candidate.')
    out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ('passed', 'frames', 'events', 'blocks')}, indent=2))


if __name__ == '__main__':
    main()

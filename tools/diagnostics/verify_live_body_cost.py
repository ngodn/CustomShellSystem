"""Verify paired in-game body bypass captures and report whole-frame differences."""
import argparse
import json
from pathlib import Path
import statistics

from capture_eve_surface_variant import atomic, sha
from verify_capped_cost import finite, verify_capture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directories', nargs='+', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--completed-pairs-only', action='store_true',
                        help='Retain a failed parent receipt and verify only its fully completed pairs')
    args = parser.parse_args()
    read = lambda p: json.loads(p.read_text())
    results, hashes, total, incomplete = [], {}, 0, []
    core, context, original_state = None, None, None
    for root in args.directories:
        report = read(root / 'result.json')
        assert report['focus_restored'] and not report.get('restore_error') and not report.get('body_restore_error')
        if not args.completed_pairs_only:
            assert report['passed'] and report['captures_complete'] and not report.get('error')
        elif not report['passed']:
            incomplete.append(dict(directory=str(root), error=report.get('error')))
        assert report['restoration'] and all(report['restoration'].values())
        state = read(root / 'state-before.json')
        assert state == read(root / 'state-after.json')
        if core is None:
            core, context, original_state = report['core_sha256'], report['context_before'], state
        assert (report['core_sha256'], report['context_before'], state) == (core, context, original_state)
        records = [json.loads(line) for line in (root / 'requests.jsonl').read_text().splitlines()]
        reflected = [(a['request'], b['result']) for a, b in zip(records, records[1:])
                     if 'request' in a and 'result' in b]
        for case in report['cases']:
            requested = report.get('requested_pairs', list(range(4)))
            captures = case['captures']
            if args.completed_pairs_only:
                pairs = [i for i in requested if sum(row.get('passed', False)
                         for row in captures if row['pair'] == i) == 2]
                captures = [row for row in captures if row['pair'] in pairs]
            else:
                assert case['passed']
                pairs = requested
            assert pairs and len(captures) == 2 * len(pairs)
            expected_order = [(i, enabled) for i in pairs
                              for enabled in ((False, True) if i % 2 == 0 else (True, False))]
            assert [(r['pair'], r['enabled']) for r in captures] == expected_order
            paired, resets, other_resets, observations = {}, [], [], []
            for row in captures:
                assert row['passed'] and finite(row['rig_before']) and finite(row['rig_after'])
                before, after = row['rig_before'], row['rig_after']
                assert before['all']['post'] == after['all']['post'] == case['post']
                old_rigs, new_rigs = before['all']['rigs'], after['all']['rigs']
                assert len(old_rigs) == len(new_rigs)
                for old, new in zip(old_rigs, new_rigs):
                    assert old['object'] == new['object'] and old['node'] == new['node']
                    for field in ('Enabled', 'Limits', 'MaxTravel'):
                        assert old['values'].get(field) == new['values'].get(field)
                    if old['object'] == before['body']['object']:
                        continue
                    if old['values'].get('Enabled', True):
                        for field in ('TotalSteps', 'SeenUpdate', 'UpdateSerial'):
                            if field in old['values']:
                                assert new['values'][field] > old['values'][field]
                    if 'ResetCount' in old['values']:
                        delta = new['values']['ResetCount'] - old['values']['ResetCount']
                        assert delta >= 0
                        if delta:
                            other_resets.append(dict(pair=row['pair'], enabled=row['enabled'],
                                                     rig=old['object'], delta=delta))
                a, b = before['body'], after['body']
                assert a['object'] == b['object']
                assert a['values']['Enabled'] is row['enabled'] and b['values']['Enabled'] is row['enabled']
                steps = b['values']['TotalSteps'] - a['values']['TotalSteps']
                assert row['body_steps'] == steps
                assert steps > 0 if row['enabled'] else steps == 0
                assert a['values']['Limits'] == b['values']['Limits']
                assert row['reset_delta'] == b['values']['ResetCount'] - a['values']['ResetCount'] >= 0
                assert any(q.get('op') == 'set' and q.get('target') == case['post'] and
                           q.get('property') == 'CSSBodyEnabled' and q.get('value') is row['enabled']
                           for q, v in reflected)
                for name, value in case['preserved_inputs'].items():
                    reads = [v for q, v in reflected if q.get('op') == 'get' and q.get('target') == case['post']
                             and q.get('property') == name]
                    assert len(reads) >= 1 + len(captures) and all(v == value for v in reads)
                path = root / row['file']
                data = read(path); verify_capture(data)
                assert data['core_sha256'] == core and data['summary'] == row['summary']
                total += data['summary']['frames']
                paired.setdefault(row['pair'], {})[row['enabled']] = data['summary']['metrics']
                if row['reset_delta']:
                    resets.append(dict(pair=row['pair'], enabled=row['enabled'], delta=row['reset_delta']))
                observations.append(dict(pair=row['pair'], enabled=row['enabled'], steps=steps,
                    rate_hz=data['summary']['engine_tick_rate_hz'], frames=data['summary']['frames']))
            differences = {metric: [paired[i][True][metric]['p50'] - paired[i][False][metric]['p50']
                                   for i in pairs] for metric in ('interval_ms', 'engine_ms', 'core_ms')}
            summaries = {metric: dict(off_median=statistics.median(v[False][metric]['p50'] for v in paired.values()),
                                     on_median=statistics.median(v[True][metric]['p50'] for v in paired.values()),
                                     paired_deltas=deltas, median_delta=statistics.median(deltas),
                                     min_delta=min(deltas), max_delta=max(deltas)) for metric, deltas in differences.items()}
            tails = {}
            for percentile in ('p95', 'p99'):
                deltas = [paired[i][True]['interval_ms'][percentile] - paired[i][False]['interval_ms'][percentile]
                          for i in pairs]
                tails[percentile] = dict(
                    off_median=statistics.median(v[False]['interval_ms'][percentile] for v in paired.values()),
                    on_median=statistics.median(v[True]['interval_ms'][percentile] for v in paired.values()),
                    paired_deltas=deltas, median_delta=statistics.median(deltas))
            results.append(dict(outfit=case['outfit'], variant=case['variant'], pairs=pairs, measurements=summaries,
                                interval_tails=tails, body_reset_events=resets, other_reset_events=other_resets,
                                observations=observations))
        for path in root.rglob('*'):
            if path.is_file() and path.resolve() != args.output.resolve() and path.name != 'SHA256SUMS':
                hashes[str(path.resolve())] = sha(path)
    identities = [(r['outfit'], r['variant']) for r in results]
    assert len(set(identities)) == len(identities), 'Duplicate appearance'
    result = dict(observation_integrity_verified=True, exclusive_solver_cost_established=False,
                  incomplete_parent_runs=incomplete,
                  full_performance_gate_closed=False, frames=total, core_sha256=core,
                  results=results, hashes=hashes)
    atomic(args.output, result)
    print(json.dumps({k: v for k, v in result.items() if k != 'hashes'}))


if __name__ == '__main__':
    main()

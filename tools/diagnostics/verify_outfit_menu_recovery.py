"""Verify recorded menu recovery coverage and its underlying reflected reads."""
import argparse
import hashlib
import json
import math
from pathlib import Path

from check_outfit_switch_lifecycle import OUTFITS
from css import atomic, sha


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def finite(value):
    if isinstance(value, dict):
        return all(finite(v) for v in value.values())
    if isinstance(value, list):
        return all(finite(v) for v in value)
    return not isinstance(value, float) or math.isfinite(value)


def verify(folders):
    expected = {(o, v) for o, variants in OUTFITS.items() for v in variants}
    covered, results, sources = set(), [], []
    baseline_state = None
    core_sha = None
    for folder in folders:
        report = json.loads((folder / 'result.json').read_text())
        before = json.loads((folder / 'state-before.json').read_text())
        after = json.loads((folder / 'state-after.json').read_text())
        assert before == after and all(report['restoration'].values())
        if baseline_state is None:
            baseline_state, core_sha = before, report['core_sha256']
        assert baseline_state == before and core_sha == report['core_sha256']
        if not report['passed']:
            assert report.get('error') == "AssertionError('Preview section visibility differs')"
            assert report['cases'][-1]['variant'] == 'bikini' and not report['cases'][-1]['passed']
        reads, calls = {}, {}
        pending = None
        count = 0
        for line in (folder / 'requests.jsonl').read_text().splitlines():
            row = json.loads(line)
            if 'request' in row:
                assert pending is None
                pending = row['request']
            elif 'result' in row:
                assert pending is not None
                target = pending.get('target', {}).get('$object')
                if pending['op'] == 'get':
                    reads.setdefault((target, pending['property']), set()).add(digest(row['result']))
                elif pending['op'] == 'call':
                    calls.setdefault((target, pending['function'], digest(pending.get('args', {}))), set()).add(digest(row['result'].get('ReturnValue')))
                pending = None
                count += 1
        assert pending is None
        for case in report['cases']:
            if not case['passed']:
                continue
            key = case['outfit'], case['variant']
            assert key in expected and key not in covered
            covered.add(key)
            for phase in ('world_before', 'preview_before', 'preview_after', 'world_after', 'world_after_later'):
                sample = case[phase]
                assert finite(sample)
                mesh, post = sample['mesh']['$object'], sample['post']['$object']
                assert digest(sample['asset']) in calls[mesh, 'GetSkeletalMeshAsset', digest({})]
                assert digest(sample['post']) in calls[mesh, 'GetPostProcessInstance', digest({})]
                if sample['lod_info'] is not None:
                    assert digest(sample['lod_info']) in reads[mesh, 'LODInfo']
                for name, value in sample['morphs'].items():
                    assert digest(value) in calls[mesh, 'GetMorphTarget', digest({'MorphTargetName': name})]
                for name, value in sample['public'].items():
                    assert digest(value) in reads[post, name]
                for rig in sample['rigs'].values():
                    for name, value in rig['values'].items():
                        assert digest(value) in reads[rig['object']['$object'], name]
            for phase in ('preview_before', 'world_after'):
                a, b = case['world_before'], case[phase]
                assert a['asset'] == b['asset'] and a['post']['class'] == b['post']['class']
                assert a['public'] == b['public'] and a['morphs'] == b['morphs']
                visibility = lambda value: {(lod, i) for lod, info in enumerate(value)
                                           for i, hidden in enumerate(info['HiddenMaterials']) if hidden}
                assert visibility(a['lod_info']) == visibility(b['lod_info'])
            phases = {}
            for left, right in (('preview_before', 'preview_after'), ('world_after', 'world_after_later')):
                a, b = case[left], case[right]
                assert a['post'] == b['post'] and set(a['rigs']) == set(b['rigs'])
                deltas = {}
                for name, rig in a['rigs'].items():
                    next_rig = b['rigs'][name]
                    assert rig['object'] == next_rig['object']
                    values, later = rig['values'], next_rig['values']
                    assert values.get('Enabled', True) == later.get('Enabled', True)
                    if not values.get('Enabled', True):
                        continue
                    counts = {k: later[k] - values[k] for k in ('SeenUpdate', 'TotalSteps', 'UpdateSerial') if k in values}
                    assert all(v > 0 for v in counts.values())
                    if counts:
                        deltas[name] = {'class': rig['object']['class'], 'counter_deltas': counts,
                                        'reset_delta': later.get('ResetCount', 0) - values.get('ResetCount', 0)}
                assert deltas
                phases[left] = deltas
            results.append(dict(outfit=key[0], variant=key[1], settings=len(case['world_before']['public']),
                                morphs=len(case['world_before']['morphs']), updates=phases))
        sources.append(dict(folder=str(folder), terminal_passed=report['passed'], recorded_calls=count,
                            hashes={name: sha(folder / name) for name in
                                    ('result.json', 'requests.jsonl', 'recipe.py', 'state-before.json', 'state-after.json')}))
    assert covered == expected, f'Missing appearances: {expected - covered}'
    return dict(passed=True, covered=len(covered), core_sha256=core_sha, sources=sources, cases=results,
                scope='All ten appearances: preview settings parity and sampled active-rig recovery after menu close. '
                      'Not continuous visual/contact, all-LOD, every attached cloth solver or other-form coverage.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folders', nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    assert not args.output.exists()
    result = verify([p.resolve() for p in args.folders])
    atomic(args.output, result)
    print(json.dumps({'passed': result['passed'], 'covered': result['covered'],
                      'reflected_reads': sum(s['recorded_calls'] for s in result['sources'])}))


if __name__ == '__main__':
    main()

"""Compare Peony's GPU correction against independently exported render morphs."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def metrics(stock, trial, expected, cloth):
    actual = trial[:, :3] - stock[:, :3]
    maximum = lambda values: float(np.linalg.norm(values, axis=1).max())
    error = maximum(actual[cloth] - expected[cloth])
    outside = maximum(actual[~cloth])
    return dict(passed=error < .05 and outside == 0., residual_max_cm=error,
                noncloth_change_max_cm=outside, expected_max_cm=maximum(expected[cloth]),
                actual_max_cm=maximum(actual[cloth]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert not args.output.exists()
    inputs = {}

    def load(path):
        raw = path.read_bytes()
        inputs[str(path.resolve())] = hashlib.sha256(raw).hexdigest()
        return json.loads(raw)

    result = load(args.stage / 'result.json')
    assert load(args.stage / 'render-exit.json')['exit_code'] == 0
    captures = result['captures']
    assert len(captures) == 44
    rows = []
    negative_checks = 0
    for mesh in ('SK_Peony', 'SK_Peony_Face'):
        structure = load(args.stage / (mesh + '-structure.json'))
        lod, = structure['render_lods']
        groups = load(args.stage / (mesh + '-cloth-morphs.json'))['groups']
        assert len(groups) == 2
        count = int(lod['vertices'])
        cloth = np.zeros(count, dtype=bool)
        shapes = {}
        start = 0
        found = set()
        for section in lod['sections']:
            end = start + int(section['vertices'])
            assert end <= count
            if section['cloth_asset_index'] >= 0:
                candidates = [g for g in groups if g['cloth_asset'] == section['cloth_asset_index']]
                group, = candidates
                assert group['slot'] not in found
                found.add(group['slot'])
                assert len(group['vertices']) == end - start
                cloth[start:end] = True
                for shape in group['morphs']:
                    values = shapes.setdefault(shape['name'], np.zeros((count, 3)))
                    for delta in shape['deltas']:
                        index = int(delta[0])
                        assert 0 <= index < end - start
                        values[start + index] = delta[1:4]
            start = end
        assert start == count and len(found) == 2 and cloth.any() and (~cloth).any()
        cases = sorted({c['case']['name'] for c in captures if c['mesh'] == mesh})
        assert len(cases) == 11
        for name in cases:
            pair = [c for c in captures if c['mesh'] == mesh and c['case']['name'] == name]
            assert len(pair) == 2 and {c['mode'] for c in pair} == {'stock', 'trial'}
            assert pair[0]['case'] == pair[1]['case']
            assert pair[0]['state']['cloth_state_sha1'] == pair[1]['state']['cloth_state_sha1']
            case = pair[0]['case']
            positions = {}
            for row in pair:
                assert row['state']['cloth_disabled'] == (not case['cloth'])
                assert not row['state']['advanced_simulation'] and row['state']['simulation_cloths'] == 2
                path = Path(row['gpu_readback'])
                data = load(path)
                assert inputs[str(path.resolve())] == row['sha256']
                values = np.asarray(data['position_normal_tangent'])
                assert data['passed'] and data['vertices'] == count and values.shape == (count, 9)
                assert np.isfinite(values).all()
                positions[row['mode']] = values
            expected = np.zeros((count, 3))
            if case['cloth']:
                for shape, weight in case['weights'].items():
                    expected += shapes[shape] * weight
            measured = metrics(positions['stock'], positions['trial'], expected, cloth)
            if name == 'PBMGlutesSize':
                assert measured['expected_max_cm'] > .1
                assert not metrics(positions['stock'], positions['stock'], expected, cloth)['passed']
                negative_checks += 1
            rows.append(dict(mesh=mesh, case=case, **measured))
    assert negative_checks == 2
    report = dict(passed=all(r['passed'] for r in rows), cases=rows, negative_checks=negative_checks,
                  inputs=inputs, scope='Held reference-pose GPU position correction and noncloth preservation. '
                  'Not gameplay, contact, motion, shading or performance acceptance.')
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(passed=report['passed'], cases=len(rows), negative_checks=negative_checks,
                         max_error_cm=max(r['residual_max_cm'] for r in rows),
                         failures=[r for r in rows if not r['passed']]), indent=2))
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()

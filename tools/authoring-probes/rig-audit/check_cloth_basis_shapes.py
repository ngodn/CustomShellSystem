"""Compare GPU cloth-basis output with independently exported render morphs."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--mesh-audit', type=Path, required=True)
    parser.add_argument('--morph-audit', type=Path, required=True)
    parser.add_argument('--run', default='2')
    args = parser.parse_args()
    assert args.run.isdecimal()
    hashes = {}

    def load(path):
        data = path.read_bytes()
        hashes[str(path)] = hashlib.sha256(data).hexdigest()
        return json.loads(data)

    receipt = load(args.stage / f'result-shapes{args.run}-readback.json')
    assert len(receipt['captures']) == 88
    audit = load(args.mesh_audit)
    rows = []
    for variant in ('Eve', 'Genessa'):
        package = f'/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_{variant}W3_Face'
        lod, = audit[package]['mesh']['render_lods']
        source = load(args.morph_audit / f'SK_{variant}W3_Face.json')
        skirt, = [g for g in source['groups'] if g['slot'] == 'UG_Skirt']
        count = len(skirt['vertices'])
        offset = 0
        candidates = []
        noncloth = np.zeros(lod['vertices'], dtype=bool)
        for section in lod['sections']:
            if section['cloth_asset_index'] == skirt['cloth_asset'] and section['vertices'] == count:
                candidates.append(offset)
            if section['cloth_asset_index'] < 0:
                noncloth[offset:offset + section['vertices']] = True
            offset += section['vertices']
        start, = candidates
        assert offset == lod['vertices']
        skirt_slice = slice(start, start + count)
        outside = np.ones(offset, dtype=bool)
        outside[skirt_slice] = False
        morphs = {}
        for morph in skirt['morphs']:
            values = np.zeros((count, 3))
            entries = np.asarray(morph['deltas'])
            if entries.size:
                values[entries[:, 0].astype(int)] = entries[:, 1:4]
            morphs[morph['name']] = values
        captures = [r for r in receipt['captures'] if r['variant'] == variant and r['view'] == 'back']
        names = sorted({r['case']['name'] for r in captures})
        assert len(names) == 11
        for name in names:
            pair = [r for r in captures if r['case']['name'] == name]
            assert len(pair) == 2 and {r['mode'] for r in pair} == {'stock', 'trial'}
            assert pair[0]['case'] == pair[1]['case']
            case = pair[0]['case']
            positions = {}
            for row in pair:
                data = load(Path(row['gpu_readback']))
                values = np.asarray(data['position_normal_tangent'])
                assert data['passed'] and values.shape == (offset, 9) and np.isfinite(values).all()
                assert row['state']['cloth_disabled'] == (not case['cloth'])
                positions[row['mode']] = values[:, :3]
            actual = positions['trial'] - positions['stock']
            expected = np.zeros((count, 3))
            if case['cloth']:
                for morph, weight in case['weights'].items():
                    expected += morphs[morph] * weight
            maximum = lambda values: float(np.linalg.norm(values, axis=1).max())
            result = dict(variant=variant, case=case, expected_max_cm=maximum(expected),
                          actual_max_cm=maximum(actual[skirt_slice]),
                          residual_max_cm=maximum(actual[skirt_slice] - expected),
                          noncloth_change_max_cm=maximum(actual[noncloth]),
                          outside_skirt_change_max_cm=maximum(actual[outside]))
            # Separate cloth runs and compressed render morphs need a numerical tolerance.
            result['passed'] = (result['residual_max_cm'] < .05 and
                                result['outside_skirt_change_max_cm'] < .02 and
                                result['noncloth_change_max_cm'] == 0.)
            rows.append(result)
    report = dict(passed=all(r['passed'] for r in rows), cases=rows, inputs=hashes,
                  scope='Reference-pose GPU shape-input and cloth-disabled equivalence only. '
                        'Not physical clearance, gameplay motion, shading or performance acceptance.')
    (args.stage / f'gpu-shapes{args.run}-comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(passed=report['passed'], cases=len(rows),
                         worst_residual_cm=max(r['residual_max_cm'] for r in rows),
                         failures=[r for r in rows if not r['passed']]), indent=2))
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()

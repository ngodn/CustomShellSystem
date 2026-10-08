"""Check isolated gait captures without treating separate cloth runs as identical."""
import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--mesh-audit', type=Path, required=True)
    parser.add_argument('--run', default='1')
    parser.add_argument('--held', action='store_true')
    args = parser.parse_args()
    assert args.run.isdecimal()
    inputs = {}

    def load(path):
        raw = path.read_bytes()
        inputs[str(path)] = hashlib.sha256(raw).hexdigest()
        return json.loads(raw)

    prefix = 'held-' if args.held else ''
    receipt = load(args.stage / f'result-{prefix}motion{args.run}-readback.json')
    audit = load(args.mesh_audit)
    assert len(receipt['captures']) == 64
    rows = []
    for variant in ('Eve', 'Genessa'):
        lod, = audit[f'/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_{variant}W3_Face']['mesh']['render_lods']
        noncloth = np.zeros(lod['vertices'], dtype=bool)
        offset = 0
        for section in lod['sections']:
            end = offset + section['vertices']
            if section['cloth_asset_index'] < 0:
                noncloth[offset:end] = True
            offset = end
        assert offset == lod['vertices']
        for gait in ('idle', 'walk', 'jog', 'sprint'):
            for shape in ('neutral', 'glute'):
                pair = {r['mode']: r for r in receipt['captures'] if r['variant'] == variant
                        and r['case']['name'] == f'{gait}-{shape}' and r['view'] == 'back'}
                assert set(pair) == {'stock', 'trial'}
                samples = pair['trial']['state']['motion_samples']
                assert samples == pair['stock']['state']['motion_samples'] and len(samples) == 4
                if args.held:
                    assert pair['stock']['state']['cloth_state_sha1'] == pair['trial']['state']['cloth_state_sha1']
                    assert all(not r['state']['advanced_simulation'] for r in pair.values())
                motion = max(math.dist(samples[0]['bones']['pelvis'], s['bones']['pelvis']) for s in samples)
                assert motion > .01
                values = {}
                for mode, row in pair.items():
                    data = load(Path(row['gpu_readback']))
                    values[mode] = np.asarray(data['position_normal_tangent'])
                    assert data['passed'] and values[mode].shape == (offset, 9) and np.isfinite(values[mode]).all()
                difference = values['trial'][:, :3] - values['stock'][:, :3]
                outside = float(np.linalg.norm(difference[noncloth], axis=1).max())
                assert outside == 0
                maximum = float(np.linalg.norm(difference, axis=1).max())
                if args.held:
                    if shape == 'neutral':
                        assert np.array_equal(values['stock'], values['trial'])
                    else:
                        # Original glute offset has magnitude <= 2.743002 cm; this
                        # loose bound catches displaced/stale cloth, not contact.
                        assert 0 < maximum < 3.0
                rows.append(dict(variant=variant, case=f'{gait}-{shape}', pelvis_motion_cm=motion,
                                 noncloth_position_change_cm=outside,
                                 render_position_change_max_cm=maximum))
    report = dict(sanity_checks_passed=True, cloth_equivalence_accepted=False, cases=rows,
                  captures=64, gpu_readbacks=32, inputs=inputs,
                  matched_state_comparison_passed=args.held,
                  scope=('Same simulated state for both graphs: neutral positions, normals and tangents are '
                         'identical; glute displacement is bounded and noncloth positions are unchanged. '
                         'No continuous contact, shading, gameplay or performance acceptance.' if args.held else
                        'Three seconds of isolated single-node gait simulation per case; finite output, '
                        'matched sampled body poses and unchanged noncloth positions. Separate neutral cloth '
                        'runs differ substantially in some cases. Hold a single simulated state for the next '
                        'graph comparison. No continuous collision, gameplay or performance acceptance.'))
    (args.stage / f'{prefix}motion{args.run}-validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('cases', 'inputs')}, indent=2))


if __name__ == '__main__':
    main()

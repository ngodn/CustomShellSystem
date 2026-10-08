"""Check the private graph edit and its position math against captured skirt morphs."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--render', type=Path, nargs='+', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    inputs = [args.baseline, args.candidate, *args.render]
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    baseline, = [a for a in json.loads(args.baseline.read_text())
                 if a['asset'].endswith('.DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals')]
    candidate = json.loads(args.candidate.read_text())
    sources = lambda a: [n['source'] for g in a['graphs'] for n in g['nodes'] if 'source' in n]
    before, after = sources(baseline), sources(candidate)
    patch = '\nFinalPosition += ClothWeight * mul((float3x3)BoneMatrix, MorphDelta);'
    assert len(before) == len(after) == 2
    assert sum(s.count(patch) for s in after) == 1
    assert sorted(before) == sorted(s.replace(patch, '') for s in after)
    assert not any('referenced_graph' in n for g in candidate['graphs'] for n in g['nodes'])

    records = []
    for path in args.render:
        skirt, = [g for g in json.loads(path.read_text())['groups'] if g['slot'] == 'UG_Skirt']
        vertices = np.asarray(skirt['vertices'], dtype=np.float32)[:, :3]
        mapping = np.asarray(skirt['cloth_mappings'][0], dtype=np.float32)
        weights = (1 - mapping[:, 15] / 65535)[:, None]
        assert np.all((weights >= 0) & (weights <= 1))
        deltas = []
        for morph in skirt['morphs']:
            delta = np.zeros_like(vertices)
            values = np.asarray(morph['deltas'], dtype=np.float32)
            delta[values[:, 0].astype(np.int64)] = values[:, 1:4]
            deltas.append((morph['name'], delta))
        deltas.append(('combined', sum(d for _, d in deltas)))
        cases = []
        for angle in (0, 45, 90):
            theta = np.deg2rad(angle)
            rotation = np.asarray([[np.cos(theta), -np.sin(theta), 0],
                                   [np.sin(theta), np.cos(theta), 0], [0, 0, 1]], dtype=np.float32)
            rest = vertices @ rotation.T
            # Synthetic cloth displacement tests preservation, not physical contact.
            motion = np.sin(np.arange(len(vertices), dtype=np.float32)[:, None] * .031)
            cloth = rest + motion * np.asarray([.3, -.4, .2], dtype=np.float32)
            for name, delta in deltas:
                posed_delta = delta @ rotation.T
                for blend in (0., .5, 1.):
                    weight = weights * blend
                    skinned = rest + posed_delta
                    baseline_position = skinned * (1 - weight) + cloth * weight
                    trial = baseline_position + weight * posed_delta
                    expected = rest * (1 - weight) + cloth * weight + posed_delta
                    residual = float(np.max(np.abs(trial - expected)))
                    assert np.isfinite(trial).all() and residual < 5e-5
                    if blend == 0:
                        assert np.array_equal(trial, skinned)
                    cases.append(dict(morph=name, angle=angle, blend=blend,
                                      residual_max_cm=residual,
                                      baseline_lost_delta_max_cm=float(np.linalg.norm(weight * posed_delta, axis=1).max())))
        assert max(c['baseline_lost_delta_max_cm'] for c in cases) > 1.
        zero = np.zeros_like(vertices)
        assert np.array_equal(cloth + weights * zero, cloth)
        records.append(dict(render=str(path), vertices=len(vertices), cases=cases))
    assert all(hashlib.sha256(p.read_bytes()).hexdigest() == hashes[str(p)] for p in inputs)
    report = dict(inputs=hashes, variants=records,
                  scope='CPU float32 position math with captured morphs and mappings, synthetic rotations and cloth motion. '
                        'Not GPU execution, posed fit, collision, tangent quality or performance acceptance.')
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Passed {sum(len(r['cases']) for r in records)} position cases across {len(records)} variants")


if __name__ == '__main__':
    main()

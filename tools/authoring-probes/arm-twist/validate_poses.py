"""Validate the bounded arm binding comparison, host Python 3.14."""
import json
import math
from pathlib import Path

import os
OUT = Path(os.environ['CSS_ARM_TWIST_AUDIT_DIR']).resolve()
if OUT.parent != Path(__file__).resolve().parents[3] / 'work/grip-grounding-v1':
    raise ValueError('Audit directory must be a direct workspace grip-grounding child')
FIXTURE = OUT.parent / 'game-foundation-v42-bind-v2'

def snapshot(path):
    data = json.loads(path.read_text())['pose']['Snapshot']
    result = dict(zip((n.lower() for n in data['BoneNames']), data['LocalTransforms'], strict=True))
    assert len(result) == len(data['BoneNames']) == 388
    return result

def errors(a, b, ignored=()):
    result = {k: 0.0 for k in ('Translation', 'Rotation', 'Scale3D')}
    assert a.keys() == b.keys()
    for name in a:
        if name in ignored:
            continue
        for key in result:
            axes = 'XYZW' if key == 'Rotation' else 'XYZ'
            v, w = [[row[name][key][axis] for axis in axes] for row in (a, b)]
            error = math.dist(v, w)
            if key == 'Rotation':
                error = min(error, math.dist(v, [-x for x in w]))
            result[key] = max(result[key], error)
    return result

helpers = {f'{part}_twist_{number:02}_{side}' for part in ('upperarm', 'lowerarm') for number in (1, 2) for side in ('l', 'r')}
rows = []
for sample in range(5):
    for path in ('raw', 'compressed'):
        for alpha in (0, 1):
            suffix = f's{sample}-{path}-ik{alpha}.json'
            a = snapshot(OUT / f'baseline_game_rotations-{suffix}')
            b = snapshot(FIXTURE / f'foundation_css_modes-{suffix}')
            baseline_error = errors(a, b)
            assert max(baseline_error.values()) < .001, baseline_error
            for policy in ('css', 'game_rotations'):
                a = snapshot(OUT / f'baseline_{policy}-{suffix}')
                b = snapshot(OUT / f'candidate_{policy}-{suffix}')
                unchanged = errors(a, b, helpers)
                assert max(unchanged.values()) < .001, unchanged
                rows.append(dict(sample=sample, path=path, alpha=alpha, policy=policy,
                                 baseline_error=baseline_error, nonhelper_pose_error=unchanged))
result = dict(prior_baseline_reproduced=True, nonhelper_poses_preserved=True, cases=len(rows), rows=rows,
              scope='Case-insensitive Unreal bone names. Changes outside eight helper translations remain below 0.001. This candidate cannot correct finger rotations or arm IK pose.')
(OUT / 'pose-validation.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps({k: v for k, v in result.items() if k != 'rows'}))

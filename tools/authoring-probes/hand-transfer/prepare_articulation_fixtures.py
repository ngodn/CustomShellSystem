"""Compare local articulation with saved full-matrix original-rig results."""
import copy
import hashlib
import json
import math
import os
import sys
from pathlib import Path
from mathutils import Quaternion

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import AUDIT, WORK, HERE, load
from articulation import apply, parameters

out = Path(os.environ['CSS_ARTICULATION_FIXTURE_DIR']).resolve()
assert out.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'fixtures.json').exists()
models = parameters(load(HERE/'left-finger-correctives-v1.json')['parameters'])
assert len(models) == 14 and sum(m['hinge'] for m in models) == 10
cases = []
maximum = 0.
for group, source_name, expected_name in (
    ('synthetic', 'left-graph-anchor-animation-stress-v2', 'left-graph-anchor-animation-articulated-v2'),
    ('captured', 'left-observed-graph-calibrated-v1', 'left-observed-graph-articulated-v1'),
):
    source, expected = AUDIT/source_name, AUDIT/expected_name
    manifest = load(expected/'manifest.json')
    assert manifest['spread_gain'] == .125
    for case in manifest['cases']:
        original = load(source/case['pose'])
        result, detail = apply(original, models)
        assert detail['valid']
        prior = load(expected/case['pose'])
        errors = []
        expected_rotations = {}
        for m in models:
            a, b = [Quaternion([d['pose']['Snapshot']['LocalTransforms'][m['index']]['Rotation'][k] for k in 'WXYZ']).normalized() for d in (result, prior)]
            q = a.rotation_difference(b)
            errors.append(math.degrees(2*math.atan2(math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z), abs(q.w))))
            expected_rotations[m['bone']] = list(a)
        maximum = max(maximum, max(errors))
        changed = {m['index'] for m in models}
        before, after = [d['pose']['Snapshot']['LocalTransforms'] for d in (original, result)]
        assert all(a == b for i, (a, b) in enumerate(zip(before, after, strict=True)) if i not in changed)
        assert all(a[k] == b[k] for a, b in zip(before, after, strict=True) for k in ('Translation', 'Scale3D'))
        cases.append(dict(label=group+'-'+case['pose'], source=str(source/case['pose']),
                          expected_wxyz=expected_rotations, singular=False,
                          saved_reference_error_degrees=max(errors), **detail))
assert len(cases) == 446 and maximum < .001, maximum
# An ambiguous X-twist input must bypass the entire stage without partial writes.
singular = copy.deepcopy(load(Path(cases[0]['source'])))
m = models[0]
q = Quaternion(m['left_wxyz']).inverted() @ Quaternion((0, 0, 1, 0)) @ Quaternion(m['right_wxyz']).inverted()
singular['pose']['Snapshot']['LocalTransforms'][m['index']]['Rotation'] = dict(zip('XYZW', (q.x, q.y, q.z, q.w)))
result, detail = apply(singular, models)
assert not detail['valid'] and result == singular
(out/'singular.json').write_text(json.dumps(singular)+'\n')
cases.append(dict(label='singular', source=str(out/'singular.json'), expected_wxyz={}, singular=True, **detail))
report = dict(scope=__doc__, parameters=models, gain=.125, cases=cases,
              driver_sha256=hashlib.sha256((HERE/'left-finger-correctives-v1.json').read_bytes()).hexdigest(),
              max_saved_reference_error_degrees=maximum, passed=True)
(out/'fixtures.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(dict(cases=len(cases), max_error=maximum, passed=True)))

"""Measure thumb-base contact boundaries by changing one source-space axis.

Offline authoring evidence only. No per-pose result is a runtime override.
"""
import copy
import json
import math
import os
import sys
from pathlib import Path
from mathutils import Quaternion

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, load, pair_set

out = Path(os.environ['CSS_THUMB_SWEEP_AUDIT_DIR']).resolve()
assert out.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out / 'report.json').exists()
assert load(WORK / 'arm-rest-batch-pose-v1/report.json')['passed']
skin = SkinFixture()
neutral = pair_set(skin.evaluate(load(AUDIT / 'left-corrected-controller-sweep-v1/control-0.json'), batch_pose=True))
m = next(m for m in skin.curves if m['bone'] == 'thumb_01_l')
rows = []
cases = load(WORK / 'arm-rest-thumb-stress-v1/report.json')['samples']
for case in cases:
    doc = load(AUDIT / 'left-graph-anchor-thumb-stress-v1' / case['pose'])
    transform = doc['pose']['Snapshot']['LocalTransforms'][m['index']]
    q = Quaternion([transform['Rotation'][k] for k in 'WXYZ']).normalized()
    original_angles = (Quaternion(m['left_wxyz']) @ q @ Quaternion(m['right_wxyz'])).normalized().to_euler(m['euler_order'])
    samples = []
    for degrees in range(0, 23, 2):
        angles = original_angles.copy()
        angles.z += math.radians(degrees)
        candidate = copy.deepcopy(doc)
        result_q = (Quaternion(m['left_wxyz']).inverted() @ angles.to_quaternion() @ Quaternion(m['right_wxyz']).inverted()).normalized()
        candidate['pose']['Snapshot']['LocalTransforms'][m['index']]['Rotation'] = dict(zip('XYZW',(result_q.x,result_q.y,result_q.z,result_q.w)))
        result = skin.evaluate(candidate, batch_pose=True)
        extra = pair_set(result)-neutral
        samples.append(dict(offset_degrees=degrees, new_pairs=len(extra), regions=result['regions']))
        if not extra and degrees > 0:
            # Keep the first passing sample for a later independent render.
            if not (out / case['pose']).exists():
                (out / case['pose']).write_text(json.dumps(candidate,indent=2)+'\n')
                (out / ('shapes-'+case['pose'])).write_text(json.dumps(dict(values=skin.shapes(candidate)),indent=2)+'\n')
    row = dict(pose=case['pose'], original_source_euler_degrees=[math.degrees(x) for x in original_angles], samples=samples)
    rows.append(row)
    print(json.dumps(row),flush=True)
skin.assert_unchanged()
(out / 'report.json').write_text(json.dumps(dict(scope=__doc__, samples=rows),indent=2)+'\n')

"""Check interpolated running-attack intervals touched by the web guard.

Interpolates the retained pre-guard poses, not a fresh game animation graph.
Uses eight subdivisions per affected frame interval, at each saved alpha.
"""
import copy
import json
import math
import os
import sys
from pathlib import Path
from mathutils import Quaternion

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, HERE, load, pair_set
from thumb_web_guard import apply

out = Path(os.environ['CSS_THUMB_TRANSITION_AUDIT_DIR']).resolve()
assert out.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out / 'report.json').exists()
assert load(WORK / 'arm-rest-thumb-web-v1/report.json')['passed']
skin = SkinFixture()
model_path = (HERE / os.environ.get('CSS_THUMB_WEB_MODEL', 'thumb-web-guard-v1.json')).resolve()
assert model_path.parent == HERE.resolve()
model = load(model_path)
driver = next(m for m in skin.curves if m['bone'] == model['bone'])
neutral = pair_set(skin.evaluate(load(AUDIT / 'left-corrected-controller-sweep-v1/control-0.json'),batch_pose=True))
rows = []
steps = []
def rotation(doc):
    t = doc['pose']['Snapshot']['LocalTransforms'][driver['index']]
    return Quaternion([t['Rotation'][k] for k in 'WXYZ']).normalized()
def angle(a,b):
    q = a.rotation_difference(b)
    return math.degrees(2*math.atan2(math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z),abs(q.w)))
for alpha in (0.0,0.5,1.0):
    for frame in range(111):
        a,b = [load(AUDIT / f'left-graph-anchor-thumb-stress-v1/running_attack-{f}-alpha-{alpha}.json') for f in (frame,frame+1)]
        ca, da = apply(a,model,driver)
        cb, db = apply(b,model,driver)
        if max(da['correction_degrees'],db['correction_degrees']) == 0:
            continue
        previous_in,previous_out = rotation(a),rotation(ca)
        for substep in range(1,9):
            fraction = substep/8
            doc = copy.deepcopy(a)
            for dst,ta,tb in zip(doc['pose']['Snapshot']['LocalTransforms'],a['pose']['Snapshot']['LocalTransforms'],b['pose']['Snapshot']['LocalTransforms'],strict=True):
                qa,qb = [Quaternion([t['Rotation'][k] for k in 'WXYZ']).normalized() for t in (ta,tb)]
                q = qa.slerp(qb,fraction).normalized()
                dst['Rotation']=dict(zip('XYZW',(q.x,q.y,q.z,q.w)))
                for field in ('Translation','Scale3D'):
                    dst[field]={k:ta[field][k]*(1-fraction)+tb[field][k]*fraction for k in 'XYZ'}
            candidate, correction = apply(doc,model,driver)
            qi,qo = rotation(doc),rotation(candidate)
            steps.append(dict(frame=frame,fraction=fraction,alpha=alpha,input_step_degrees=angle(previous_in,qi),output_step_degrees=angle(previous_out,qo)))
            previous_in,previous_out = qi,qo
            if substep == 8:
                continue
            result = skin.evaluate(candidate,batch_pose=True)
            extra = pair_set(result)-neutral
            row=dict(frame=frame,fraction=fraction,alpha=alpha,new_pairs=len(extra),regions=result['regions'],**correction)
            rows.append(row)
            if extra:
                label=f'{frame}-{substep}-alpha-{alpha}.json'
                (out/label).write_text(json.dumps(candidate,indent=2)+'\n')
                (out/('contacts-'+label)).write_text(json.dumps(result,indent=2)+'\n')
                (out/('shapes-'+label)).write_text(json.dumps(dict(values=skin.shapes(candidate)),indent=2)+'\n')
                print(json.dumps(row),flush=True)
skin.assert_unchanged()
passed=not any(r['new_pairs'] or r['correction_saturated'] for r in rows)
report=dict(scope=__doc__,model=model,passed=passed,samples=rows,steps=steps,failures=sum(r['new_pairs']>0 for r in rows),
            maximum_added_step_degrees=max(r['output_step_degrees']-r['input_step_degrees'] for r in steps))
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('samples','steps')},indent=2),flush=True)
raise SystemExit(0 if passed else 2)

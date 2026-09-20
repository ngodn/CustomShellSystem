"""Archive post-clearance fixtures for the native web/corrective stage.

Inputs already include calibration and older finger/tip clearance. This does
not claim those preceding stages execute in the native graph being tested.
"""
import copy
import json
import os
import sys
from pathlib import Path
from mathutils import Quaternion

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import WORK, HERE, AUDIT, load
from thumb_web_guard import apply

out = Path(os.environ['CSS_NATIVE_WEB_FIXTURE_DIR']).resolve()
assert out.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'fixtures.json').exists()
web = load(HERE/'thumb-web-guard-v2.json')
curves = load(HERE/'left-finger-correctives-v1.json')['parameters']
driver = next(m for m in curves if m['bone']==web['bone'])
saved = load(WORK/'arm-rest-thumb-web-v2/report.json')
transitions = load(WORK/'arm-rest-thumb-web-transitions-v2/report.json')
assert saved['passed'] and transitions['passed']
cases = []

def add(label, doc):
    candidate, details = apply(doc,web,driver)
    values = {}
    transforms = candidate['pose']['Snapshot']['LocalTransforms']
    for m in curves:
        q = Quaternion([transforms[m['index']]['Rotation'][k] for k in 'WXYZ']).normalized()
        source = (Quaternion(m['left_wxyz'])@q@Quaternion(m['right_wxyz'])).normalized()
        angle = source.to_euler(m['euler_order'])['XYZ'.index(m['axis'])]
        values[m['shape']] = max(0,min(1,m['coefficient']*angle))-m['baked_value']
    cases.append(dict(label=label, snapshot=doc['pose']['Snapshot'],
        expected_rotation_xyzw=[transforms[driver['index']]['Rotation'][k] for k in 'XYZW'],
        expected_curves=values, **details))

for r in saved['samples']:
    add(r['group']+'-'+r['label'],load(Path(r['source'])))
for r in transitions['samples']:
    a,b = [load(AUDIT/f"left-graph-anchor-thumb-stress-v1/running_attack-{f}-alpha-{r['alpha']}.json") for f in (r['frame'],r['frame']+1)]
    doc=copy.deepcopy(a)
    fraction=r['fraction']
    for dst,ta,tb in zip(doc['pose']['Snapshot']['LocalTransforms'],a['pose']['Snapshot']['LocalTransforms'],b['pose']['Snapshot']['LocalTransforms'],strict=True):
        qa,qb = [Quaternion([t['Rotation'][k] for k in 'WXYZ']).normalized() for t in (ta,tb)]
        q=qa.slerp(qb,fraction).normalized()
        dst['Rotation']=dict(zip('XYZW',(q.x,q.y,q.z,q.w)))
        for field in ('Translation','Scale3D'):
            dst[field]={k:ta[field][k]*(1-fraction)+tb[field][k]*fraction for k in 'XYZ'}
    add(f"transition-{r['frame']}-{fraction}-alpha-{r['alpha']}",doc)
assert len(cases)==569
(out/'fixtures.json').write_text(json.dumps(dict(scope=__doc__,cases=cases),indent=2)+'\n')
print(json.dumps(dict(fixtures=len(cases),changed=sum(r['correction_degrees']>0 for r in cases))),flush=True)

"""Prepare all B2 articulated/control inputs for native four-finger clearance."""
import hashlib
import json
import os
import sys
from pathlib import Path
from mathutils import Matrix, Quaternion, Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import AUDIT, WORK, load
from directional_clearance import solve

out=Path(os.environ['CSS_NATIVE_CLEARANCE_FIXTURES']).resolve()
assert out.parent==WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'fixtures.json').exists()
model_path=WORK/'hand-clearance-b2-fit-v1/model.json'
model=load(model_path)
iterations=int(os.environ.get('CSS_FINGER_ITERATION_LIMIT','1'))
assert 1<=iterations<=model['iteration_limit']
gradient_step=float(os.environ.get('CSS_CLEARANCE_GRADIENT_DEGREES','1'))
assert .01<=gradient_step<=1
model=dict(model,iteration_limit=iterations,gradient_step_degrees=gradient_step)
inputs=[]
for group,directory in [('synthetic','left-graph-anchor-animation-articulated-v2'),('captured','left-observed-graph-articulated-v1')]:
    manifest=load(AUDIT/directory/'manifest.json')
    assert manifest['spread_gain']==.125
    inputs += [(group+'-'+c['pose'],AUDIT/directory/c['pose']) for c in manifest['cases']]
inputs += [(f'control-{i}',AUDIT/f'left-corrected-controller-sweep-v1/control-{i}.json') for i in range(0,61,5)]
inputs += [(f'h2-{i}',WORK/f'arm-rest-hand-transfer-v1/all-h2-samples-v2/h2-s{i}-calibrated.json') for i in range(5)]
cases=[]
for label,path in inputs:
    doc=load(path)
    result,detail=solve(doc,model,'fingers')
    world=[]
    for b,t in zip(model['bones'],doc['pose']['Snapshot']['LocalTransforms'][:379],strict=True):
        m=Matrix.LocRotScale(Vector(b['translation']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
        world.append(world[b['parent']]@m if b['parent']>=0 else m)
    # FTransform composition is exact for the tested uniformly scaled hand
    # chains. Arbitrary nonuniform ancestors can introduce unsupported shear.
    for m in model['models'][1:]:
        for i in [model['bones'][m['indices'][0]]['parent'],*m['indices']]:
            s=world[i].to_scale();assert max(abs(v-1) for v in s)<1e-5
    expected={model['bones'][m['indices'][0]]['name']:[result['pose']['Snapshot']['LocalTransforms'][m['indices'][0]]['Rotation'][k] for k in 'XYZW'] for m in model['models'][1:]}
    cases.append(dict(label=label,source=str(path),source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),expected_xyzw=expected,reference=detail))
assert len(cases)==464
(out/'fixtures.json').write_text(json.dumps(dict(scope=__doc__,model_sha256=hashlib.sha256(model_path.read_bytes()).hexdigest(),finger_iteration_limit=iterations,gradient_step_degrees=gradient_step,cases=cases),indent=2)+'\n')
print('Prepared',len(cases),'cases')

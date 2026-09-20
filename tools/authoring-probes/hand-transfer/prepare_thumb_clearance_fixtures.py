"""Thumb-tip reference after measured native finger clearance on B2 poses."""
import copy
import hashlib
import json
import os
import sys
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import WORK, load
from directional_clearance import solve

out=Path(os.environ['CSS_NATIVE_CLEARANCE_FIXTURES']).resolve()
assert out.parent==WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'fixtures.json').exists()
native_dir=Path(os.environ.get('CSS_NATIVE_FINGER_RESULT_DIR',str(WORK/'hand-native-finger-clearance-one-v3'))).resolve()
assert native_dir.parent==WORK.resolve()
report_path=native_dir/'report.json'
report=load(report_path)
assert report['passed'] and load(native_dir/'exit.json')['exit_code']==0
fixture_path=Path(report['fixture_path'])
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert digest(fixture_path)==report['fixture_sha256']
fixtures=load(fixture_path)
native={c['label']:c for c in report['cases'] if c['enabled'] and c['valid'] and not c['antipodes']}
model_path=WORK/'hand-clearance-b2-fit-v1/model.json'
assert fixtures['model_sha256']==digest(model_path)
model=load(model_path)
iterations=int(os.environ.get('CSS_THUMB_ITERATION_LIMIT',str(model['iteration_limit'])))
epsilon=float(os.environ.get('CSS_CLEARANCE_GRADIENT_DEGREES',str(model['gradient_step_degrees'])))
assert 1<=iterations<=model['iteration_limit'] and .01<=epsilon<=1
model=dict(model,iteration_limit=iterations,gradient_step_degrees=epsilon)
thumb=model['models'][0]['indices'][0]
index={b['name']:i for i,b in enumerate(model['bones'])}
cases=[]
for case in fixtures['cases']:
    source=Path(case['source'])
    assert digest(source)==case['source_sha256']
    doc=copy.deepcopy(load(source))
    measured=native[case['label']]['output_rotations_xyzw']
    assert set(measured)==set(case['expected_xyzw'])
    for name,q in measured.items():
        doc['pose']['Snapshot']['LocalTransforms'][index[name]]['Rotation']=dict(zip('XYZW',q))
    path=out/(case['label']+'.json')
    path.write_text(json.dumps(doc)+'\n')
    result,detail=solve(doc,model,'thumb')
    expected={model['bones'][thumb]['name']:[result['pose']['Snapshot']['LocalTransforms'][thumb]['Rotation'][k] for k in 'XYZW']}
    cases.append(dict(label=case['label'],source=str(path),source_sha256=digest(path),
                      articulated_source=str(source),articulated_source_sha256=digest(source),
                      expected_xyzw=expected,reference=detail))
assert len(cases)==464
(out/'fixtures.json').write_text(json.dumps(dict(scope=__doc__,stage='thumb',
    model_sha256=digest(model_path),iteration_limit=iterations,gradient_step_degrees=epsilon,
    native_finger_report=str(report_path),native_finger_report_sha256=digest(report_path),
    finger_fixture_sha256=digest(fixture_path),cases=cases),indent=2)+'\n')
print('Prepared',len(cases),'thumb-tip cases after measured native fingers')

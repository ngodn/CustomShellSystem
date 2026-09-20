"""Replay measured native clearance, then remaining offline stages, on B2.

A complete but numerically rejected native report may be inspected here.
A skin pass does not override that rejection or establish acceptable cost.
"""
import hashlib
import json
import os
import sys
import time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture,AUDIT,WORK,HERE,load,pair_set
from directional_clearance import solve
from thumb_web_guard import apply

out=Path(os.environ['CSS_NATIVE_CLEARANCE_SKIN_DIR']).resolve()
native=Path(os.environ['CSS_NATIVE_CLEARANCE_RESULT_DIR']).resolve()
assert out.parent==native.parent==WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'report.json').exists()
report=load(native/'report.json')
stage=report.get('stage','fingers')
assert stage in ('fingers','thumb')
assert len(report['cases']) in (933,934)
assert load(native/'exit.json')['exit_code'] in (0,255)
fixture_path=Path(report.get('fixture_path',str(WORK/'hand-native-clearance-fixtures-v1/fixtures.json'))).resolve()
assert fixture_path.parent.parent==WORK.resolve()
assert report['fixture_sha256']==hashlib.sha256(fixture_path.read_bytes()).hexdigest()
fixtures=load(fixture_path)['cases']
measured={r['label']:r for r in report['cases'] if r['enabled'] and not r['antipodes']}
model=load(WORK/'hand-clearance-b2-fit-v1/model.json')
web=load(HERE/'thumb-web-guard-v2.json')
skin=SkinFixture();assert skin.source_hash==model['blend_sha256']
driver=next(m for m in skin.curves if m['bone']==web['bone'])
neutral=pair_set(skin.evaluate(load(AUDIT/'left-corrected-controller-sweep-v1/control-0.json'),batch_pose=True))
assert len(neutral)==4
rows=[];started=time.monotonic()
for i,case in enumerate(fixtures):
    path=Path(case['source']);assert hashlib.sha256(path.read_bytes()).hexdigest()==case['source_sha256']
    doc=load(path);row=measured[case['label']];assert row['valid']
    for name,rotation in row['output_rotations_xyzw'].items():
        doc['pose']['Snapshot']['LocalTransforms'][skin.index[name]]['Rotation']=dict(zip('XYZW',rotation))
    if stage=='fingers':
        thumb,tr=solve(doc,model,'thumb')
    else:
        thumb,tr=doc,dict(stage='thumb',source='measured native output')
    result,wr=apply(thumb,web,driver)
    contact=skin.evaluate(result,batch_pose=True)
    extra=pair_set(contact)-neutral
    rows.append(dict(label=case['label'],new_pairs=len(extra),thumb=tr,web=wr))
    (out/(case['label']+'.json')).write_text(json.dumps(result)+'\n')
    (out/('shapes-'+case['label']+'.json')).write_text(json.dumps(dict(values=skin.shapes(result)))+'\n')
    if extra:(out/('contacts-'+case['label']+'.json')).write_text(json.dumps(contact,indent=2)+'\n')
    if extra or i%50==0:print(json.dumps(dict(progress=i+1,label=case['label'],new_pairs=len(extra))),flush=True)
skin.assert_unchanged()
passed=not any(r['new_pairs'] or r['web']['correction_saturated'] for r in rows)
result=dict(scope=__doc__,stage=stage,passed=passed,samples=rows,failures=sum(bool(r['new_pairs']) for r in rows),
            native_report_sha256=hashlib.sha256((native/'report.json').read_bytes()).hexdigest(),
            native_numerical_gate_passed=report['passed'],seconds=time.monotonic()-started)
(out/'report.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='samples'}),flush=True)
raise SystemExit(0 if passed else 2)

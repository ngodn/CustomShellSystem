"""Recompute finger, thumb-tip and web correction from articulated B2 input.

Actual skin verifies the result. Prior corrections are not reused as outputs.
The diagnostic still lacks weapon contacts and a native directional solver.
"""
import json
import hashlib
import math
import os
import sys
import time
from pathlib import Path
from mathutils import Quaternion

sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, HERE, load, pair_set
from directional_clearance import solve
from thumb_web_guard import apply

out=Path(os.environ['CSS_B2_CLEARANCE_AUDIT_DIR']).resolve()
assert out.parent==WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'report.json').exists()
model=load(WORK/'hand-clearance-b2-fit-v1/model.json')
finger_iterations=int(os.environ.get('CSS_FINGER_ITERATION_LIMIT',model['iteration_limit']))
assert 1<=finger_iterations<=model['iteration_limit']
finger_model=dict(model,iteration_limit=finger_iterations)
web=load(HERE/'thumb-web-guard-v2.json')
skin=SkinFixture()
assert skin.source_hash==model['blend_sha256']
driver=next(m for m in skin.curves if m['bone']==web['bone'])
neutral=pair_set(skin.evaluate(load(AUDIT/'left-corrected-controller-sweep-v1/control-0.json'),batch_pose=True))
assert len(neutral)==4
cases=[]
if os.environ.get('CSS_B2_CLEARANCE_SYNTHETIC')=='1':
    articulated=load(AUDIT/'left-graph-anchor-animation-articulated-v2/manifest.json')
    assert articulated['spread_gain']==.125, 'Clearance requires the verified articulation stage'
    for c in articulated['cases']:
        cases.append(dict(group='synthetic',label=c['pose'],source=AUDIT/'left-graph-anchor-animation-articulated-v2'/c['pose']))
articulated=load(AUDIT/'left-observed-graph-articulated-v1/manifest.json')
assert articulated['spread_gain']==.125, 'Clearance requires the verified articulation stage'
for c in articulated['cases']:
    cases.append(dict(group='captured',label=c['pose'],source=AUDIT/'left-observed-graph-articulated-v1'/c['pose']))
for degrees in range(0,61,5):
    cases.append(dict(group='control',label=f'control-{degrees}.json',source=AUDIT/f'left-corrected-controller-sweep-v1/control-{degrees}.json'))
for i in range(5):
    cases.append(dict(group='h2',label=f'h2-{i}.json',source=WORK/f'arm-rest-hand-transfer-v1/all-h2-samples-v2/h2-s{i}-calibrated.json'))
changed={skin.index[n] for n in ('thumb_01_l','index_01_l','middle_01_l','ring_01_l','pinky_01_l')}
rows=[]
native_path=os.environ.get('CSS_B2_NATIVE_ARTICULATION_DIR')
native_rows={}
if native_path:
    native_dir=Path(native_path).resolve()
    assert native_dir.parent==WORK.resolve()
    native_report=load(native_dir/'report.json')
    assert load(native_dir/'exit.json')['exit_code']==0 and native_report['passed']
    fixtures=load(WORK/'hand-articulation-fixtures-v1/fixtures.json')
    native_fixtures={c['label']:c for c in fixtures['cases']}
    for row in native_report['cases']:
        if row['enabled'] and row['valid'] and not row['antipodes'] and row['label'] in native_fixtures:
            native_rows[row['label']]=row
    assert len(native_rows)==446
started=time.monotonic()
for i,case in enumerate(cases):
    doc=load(case['source'])
    label=case['group']+'-'+case['label']
    if native_path and case['group'] in ('synthetic','captured'):
        # Reconstruct from the pre-articulation input and measured native locals.
        # Never substitute a recomputed expected articulation result here.
        doc=load(Path(native_fixtures[label]['source']))
        for name,rotation in native_rows[label]['output_rotations_xyzw'].items():
            doc['pose']['Snapshot']['LocalTransforms'][skin.index[name]]['Rotation']=dict(zip('XYZW',rotation))
    fingers,finger_report=solve(doc,finger_model,'fingers')
    thumb,thumb_report=solve(fingers,model,'thumb')
    result,web_report=apply(thumb,web,driver)
    before=doc['pose']['Snapshot']['LocalTransforms'];after=result['pose']['Snapshot']['LocalTransforms']
    assert all(a==b for n,(a,b) in enumerate(zip(before,after,strict=True)) if n not in changed)
    assert all(a[k]==b[k] for a,b in zip(before,after,strict=True) for k in ('Translation','Scale3D'))
    contact=skin.evaluate(result,batch_pose=True)
    extra=pair_set(contact)-neutral
    errors=[]
    if i%10==0:
        full,_=solve(doc,finger_model,'fingers',full_radials=True)
        full,_=solve(full,model,'thumb',full_radials=True)
        full,_=apply(full,web,driver)
        for n in changed:
            q1,q2=[Quaternion([d['pose']['Snapshot']['LocalTransforms'][n]['Rotation'][k] for k in 'WXYZ']).normalized() for d in (result,full)]
            q=q1.rotation_difference(q2)
            errors.append(math.degrees(2*math.atan2(math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z),abs(q.w))))
    row=dict(group=case['group'],label=case['label'],source=str(case['source']),new_pairs=len(extra),regions=contact['regions'],
             fingers=finger_report,thumb=thumb_report,web=web_report,max_hull_rotation_error_degrees=max(errors,default=0),hull_compared=bool(errors))
    (out/label).write_text(json.dumps(result,indent=2)+'\n')
    (out/('shapes-'+label)).write_text(json.dumps(dict(values=skin.shapes(result)),indent=2)+'\n')
    if extra: (out/('contacts-'+label)).write_text(json.dumps(contact,indent=2)+'\n')
    rows.append(row)
    if extra or i%25==0: print(json.dumps(dict(progress=i+1,label=label,new_pairs=len(extra),hull_error=max(errors,default=0))),flush=True)
skin.assert_unchanged()
groups={g:dict(total=sum(r['group']==g for r in rows),failures=sum(r['group']==g and r['new_pairs']>0 for r in rows)) for g in sorted({r['group'] for r in rows})}
passed=not any(r['new_pairs'] or r['max_hull_rotation_error_degrees']>.001 or r['web']['correction_saturated'] for r in rows)
(out/'report.json').write_text(json.dumps(dict(scope=__doc__,passed=passed,samples=rows,groups=groups,seconds=time.monotonic()-started,
    native_articulation_report_sha256=hashlib.sha256((native_dir/'report.json').read_bytes()).hexdigest() if native_path else None,
    native_articulation_cases=len(native_rows),finger_iteration_limit=finger_iterations),indent=2)+'\n')
print(json.dumps(dict(passed=passed,groups=groups,seconds=time.monotonic()-started)),flush=True)
raise SystemExit(0 if passed else 2)

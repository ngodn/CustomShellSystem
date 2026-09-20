"""Recompute articulation and all clearance after interpolating calibrated input.

Samples every running interval midpoint at three overlay alphas, plus seven
interior samples in the earlier thumb-regression intervals. This is not a
complete game animation graph or a proof over continuous time.
"""
import copy
import json
import math
import os
import sys
from pathlib import Path
from mathutils import Quaternion
sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture,AUDIT,WORK,HERE,load,pair_set
from articulation import apply as articulate, parameters
from directional_clearance import solve
from thumb_web_guard import apply as web_guard

out=Path(os.environ['CSS_FRESH_HAND_TRANSITION_DIR']).resolve()
assert out.parent==WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'report.json').exists()
model=load(WORK/'hand-clearance-b2-fit-v1/model.json')
gradient_step=float(os.environ.get('CSS_CLEARANCE_GRADIENT_DEGREES','1'))
assert .01<=gradient_step<=1
finger=dict(model,iteration_limit=1,gradient_step_degrees=gradient_step)
web=load(HERE/'thumb-web-guard-v2.json')
skin=SkinFixture();assert skin.source_hash==model['blend_sha256']
articulation=parameters(skin.curves)
driver=next(m for m in skin.curves if m['bone']==web['bone'])
neutral=pair_set(skin.evaluate(load(AUDIT/'left-corrected-controller-sweep-v1/control-0.json'),batch_pose=True))
assert len(neutral)==4
old=load(WORK/'arm-rest-thumb-web-transitions-v2/report.json')
affected={(r['frame'],r['alpha']) for r in old['samples']}
assert len(affected)==15
rows=[];steps=[]
changed=[skin.index[n] for n in ('thumb_01_l','index_01_l','middle_01_l','ring_01_l','pinky_01_l')]

def pipeline(doc):
    doc,ar=articulate(doc,articulation);assert ar['valid']
    doc,fr=solve(doc,finger,'fingers')
    doc,tr=solve(doc,model,'thumb')
    doc,wr=web_guard(doc,web,driver)
    return doc,dict(articulation=ar,fingers=fr,thumb=tr,web=wr)

def angle(a,b,index):
    qa,qb=[Quaternion([d['pose']['Snapshot']['LocalTransforms'][index]['Rotation'][k] for k in 'WXYZ']).normalized() for d in (a,b)]
    q=qa.rotation_difference(qb)
    return math.degrees(2*math.atan2(math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z),abs(q.w)))

for alpha in (0.,.5,1.):
    for frame in range(111):
        a,b=[load(AUDIT/f'left-graph-anchor-animation-stress-v2/running_attack-{f}-alpha-{alpha}.json') for f in (frame,frame+1)]
        previous_in=a;previous_out,_=pipeline(a)
        fractions=[i/8 for i in range(1,8)] if (frame,alpha) in affected else [.5]
        for fraction in [*fractions,1.]:
            doc=copy.deepcopy(a)
            for dst,ta,tb in zip(doc['pose']['Snapshot']['LocalTransforms'],a['pose']['Snapshot']['LocalTransforms'],b['pose']['Snapshot']['LocalTransforms'],strict=True):
                qa,qb=[Quaternion([t['Rotation'][k] for k in 'WXYZ']).normalized() for t in (ta,tb)]
                q=qa.slerp(qb,fraction).normalized()
                dst['Rotation']=dict(zip('XYZW',(q.x,q.y,q.z,q.w)))
                for field in ('Translation','Scale3D'):dst[field]={k:ta[field][k]*(1-fraction)+tb[field][k]*fraction for k in 'XYZ'}
            result,detail=pipeline(doc)
            steps.append(dict(frame=frame,alpha=alpha,fraction=fraction,
                maximum_added_rotation_degrees=max(angle(previous_out,result,i)-angle(previous_in,doc,i) for i in changed)))
            previous_in,previous_out=doc,result
            if fraction==1.:continue
            contact=skin.evaluate(result,batch_pose=True);extra=pair_set(contact)-neutral
            row=dict(frame=frame,alpha=alpha,fraction=fraction,new_pairs=len(extra),**detail)
            rows.append(row)
            if extra or detail['web']['correction_saturated']:
                label=f'{frame}-{fraction}-alpha-{alpha}.json'
                (out/label).write_text(json.dumps(result,indent=2)+'\n')
                (out/('contacts-'+label)).write_text(json.dumps(contact,indent=2)+'\n')
                (out/('shapes-'+label)).write_text(json.dumps(dict(values=skin.shapes(result)))+'\n')
                print(json.dumps(dict(frame=frame,alpha=alpha,fraction=fraction,new_pairs=len(extra))),flush=True)
        if frame%25==0:print(json.dumps(dict(alpha=alpha,frame=frame,samples=len(rows))),flush=True)
skin.assert_unchanged()
assert len(rows)==423
passed=not any(r['new_pairs'] or r['web']['correction_saturated'] for r in rows)
report=dict(scope=__doc__,passed=passed,samples=rows,steps=steps,failures=sum(bool(r['new_pairs']) for r in rows),
    finger_iterations=1,gradient_step_degrees=gradient_step,maximum_added_step_degrees=max(s['maximum_added_rotation_degrees'] for s in steps))
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('samples','steps')}),flush=True)
raise SystemExit(0 if passed else 2)

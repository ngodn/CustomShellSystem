"""Compare the rewritten directional solver with saved original V43 results."""
import json
import math
import os
import sys
from pathlib import Path
from mathutils import Quaternion,Vector

sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import MOD,AUDIT,WORK,load
from export_seduxtress_eve import read_bones
from directional_clearance import solve

out=Path(os.environ['CSS_CLEARANCE_PORT_AUDIT_DIR']).resolve()
assert out.parent==WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'report.json').exists()
bones,bind=read_bones(MOD/'work/CSS_SeduXtress_HandBindV43.bindpose.json')
index={b['name']:i for i,b in enumerate(bones)}
old=load(AUDIT/'left-graph-anchor-thumb-stress-v1-solver.json')['models']
models=[]
for item in old:
    ids=[index[f"{item['finger']}_{s:02d}_l"] for s in (1,2,3)]
    radials=[[list(bind[ids[s]].to_quaternion().inverted()@Vector(v)) for v in values] for s,values in enumerate(item['radials'])]
    models.append(dict(item,indices=ids,radials_local=radials,hull_radials_local=radials))
p=lambda n:bind[index[n]].translation
normal=(p('middle_01_l')-p('hand_l')).cross(p('index_01_l')-p('pinky_01_l')).normalized()
model=dict(bones=bones,models=models,palm_normal_local=list(bind[index['hand_l']].to_quaternion().inverted()@normal),
    translation_policy='captured',margin_cm=.01,gradient_step_degrees=.05,iteration_limit=12,iteration_step_degrees=2,correction_limit_degrees=12)
model.update(legacy_bind=[[list(row) for row in m] for m in bind],
    legacy_hand_inverse=list(bind[index['hand_l']].to_quaternion().inverted()),legacy_palm_normal=list(normal))
cases=[]
for name in ('shared_idle-0-alpha-1.0.json','running_attack-5-alpha-1.0.json','running_attack-75-alpha-0.0.json','heavy_attack-106-alpha-1.0.json'):
    cases.append((name,AUDIT/'left-graph-anchor-animation-articulated-v2'/name,AUDIT/'left-graph-anchor-animation-directional-v3'/name,AUDIT/'left-graph-anchor-thumb-stress-v1'/name))
for name in ('hand-overlay-observation-v2-pose-11.json','hand-overlay-observation-v2-pose-16.json'):
    cases.append((name,AUDIT/'left-observed-graph-articulated-v1'/name,AUDIT/'left-observed-graph-directional-v1'/name,AUDIT/'left-observed-thumb-clearance-v1'/name))

def error(a,b):
    values=[]
    for name in ('thumb_01_l','index_01_l','middle_01_l','ring_01_l','pinky_01_l'):
        qa,qb=[Quaternion([doc['pose']['Snapshot']['LocalTransforms'][index[name]]['Rotation'][k] for k in 'WXYZ']).normalized() for doc in (a,b)]
        q=qa.rotation_difference(qb)
        values.append(math.degrees(2*math.atan2(math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z),abs(q.w))))
    return max(values)

rows=[]
for label,source,fingers_expected,thumb_expected in cases:
    original=load(source)
    fingers,fr=solve(original,model,'fingers')
    thumb,tr=solve(load(fingers_expected),model,'thumb')
    row=dict(label=label,finger_error_degrees=error(fingers,load(fingers_expected)),thumb_error_degrees=error(thumb,load(thumb_expected)),fingers=fr,thumb=tr)
    variants={}
    for mode in ('palm', 'radial', 'both'):
        model['legacy_palm_composition']=mode in ('palm','both')
        candidate,detail=solve(original,model,'fingers',legacy_transport=mode in ('radial','both'))
        variants[mode]=dict(error_degrees=error(candidate,load(fingers_expected)),iterations=detail['iterations'],final_gap_cm=detail['final_gap_cm'])
    model['legacy_palm_composition']=False
    row['rounding_isolation']=variants
    rows.append(row);print(json.dumps(row),flush=True)
passed=all(max(r['finger_error_degrees'],r['thumb_error_degrees'])<.001 for r in rows)
(out/'report.json').write_text(json.dumps(dict(scope=__doc__,passed=passed,samples=rows),indent=2)+'\n')
(out/'legacy-model.json').write_text(json.dumps(model,indent=2)+'\n')
raise SystemExit(0 if passed else 2)

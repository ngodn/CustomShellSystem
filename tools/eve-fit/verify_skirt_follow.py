"""Check carrier mapping against fitted skinning at an actual recorded pose."""
import argparse
import copy
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix,Quaternion,Vector

work=Path(__file__).resolve().parents[2]/'work/eve26'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--motion',type=Path,required=True)
p.add_argument('--frame',type=int,required=True)
p.add_argument('--name',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert a.name.isalnum()
out=work/f'{a.name}-follow-pose.json';assert not out.exists()
source=json.loads((work/'holiday-hip-clean.mesh.json').read_text())
trial=json.loads((work/'holiday-follow.mesh.json').read_text())
mapping=json.loads((work/'skirt-follow.json').read_text())['drivers']
frame=copy.deepcopy(json.loads(a.motion.read_text())['frames'][a.frame])
snapshot=frame['upstream']
entries=dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
indices={b['name']:i for i,b in enumerate(source['bones'])}
bind=[];pose=[]
for bone in source['bones']:
    q=bone['rotation']
    b=Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    e=entries[bone['name']]
    m=Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),Quaternion([e['Rotation'][k] for k in 'WXYZ']),Vector([e['Scale3D'][k] for k in 'XYZ']))
    parent=bone['parent']
    if parent>=0:b=bind[parent]@b;m=pose[parent]@m
    bind.append(b);pose.append(m)
modified=list(pose)
for target,driver in mapping.items():
    t=indices[target];d=indices[driver]
    modified[t]=pose[d]@bind[d].inverted()@bind[t]
# Serialize through local TRS, as the engine will, then measure its actual error.
result=copy.deepcopy(snapshot)
local_by_name={}
for i,bone in enumerate(source['bones']):
    parent=bone['parent']
    m=modified[parent].inverted()@modified[i] if parent>=0 else modified[i]
    t,q,s=m.decompose()
    local_by_name[bone['name']]={'Translation':dict(zip('XYZ',t)),
        'Rotation':dict(zip('XYZW',[q.x,q.y,q.z,q.w])),'Scale3D':dict(zip('XYZ',s))}
result['LocalTransforms']=[local_by_name.get(n,e) for n,e in zip(result['BoneNames'],result['LocalTransforms'],strict=True)]
rebuilt=[]
for bone in source['bones']:
    e=local_by_name[bone['name']]
    m=Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),Quaternion([e['Rotation'][k] for k in 'WXYZ']),Vector([e['Scale3D'][k] for k in 'XYZ']))
    if bone['parent']>=0:m=rebuilt[bone['parent']]@m
    rebuilt.append(m)
def skin(mesh,transforms,points):
    mats=[np.asarray(m@b.inverted()) for m,b in zip(transforms,bind)]
    result=np.zeros_like(points)
    for v,b,w in mesh['influences']:
        result[v]+=w*(mats[b][:3,:3]@points[v]+mats[b][:3,3])
    return result
reports={}
for label,weights in [('default',{}),('hip-waist',{'PBMHipSize':1.,'PBMWaistWidth':1.})]:
    points=np.asarray(source['points'],dtype=float)
    for morph in source['morph_targets']:
        amount=weights.get(morph['name'],0.)
        if amount:
            for i,*delta in morph['deltas']:points[i]+=np.asarray(delta)*amount
    error=np.linalg.norm(skin(source,pose,points)-skin(trial,rebuilt,points),axis=1)
    reports[label]={'max_cm':float(error.max()),'p95_cm':float(np.percentile(error,95))}
    assert error.max()<.001,reports[label]
frame['pose']['Snapshot']=result
scope='Offline bind-compensated driver transforms on measured upstream animation. No secondary dynamics or game validation.'
out.write_text(json.dumps({'scope':scope,'frames':[frame]},separators=(',',':')))
(work/f'{a.name}-follow-verified.json').write_text(json.dumps({'scope':scope,'source_motion':str(a.motion),'frame':a.frame,'cases':reports},indent=2)+'\n')
print(reports)

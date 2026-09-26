"""Measure animation-follow drivers before adding skirt secondary motion."""
import argparse
import copy
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector

work=Path(__file__).resolve().parents[2]/'work/eve26'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--motion',type=Path,required=True)
p.add_argument('--frame',type=int,required=True)
p.add_argument('--name',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert a.name.isalnum()
output=work/f'{a.name}-drivers.json'
assert not output.exists()
source=json.loads((work/'holiday-hip-clean.mesh.json').read_text())
trial=json.loads((work/'holiday-hip-bones.mesh.json').read_text())
assert source['points']==trial['points'] and source['bones']==trial['bones']
record=json.loads(a.motion.read_text())['frames'][a.frame]
snapshot=record['upstream']
pose_by_name=dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
bind=[];posed=[]
for b in source['bones']:
    q=b['rotation']
    m=Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
    s=pose_by_name[b['name']]
    v=Matrix.LocRotScale(Vector([s['Translation'][k] for k in 'XYZ']),Quaternion([s['Rotation'][k] for k in 'WXYZ']),Vector([s['Scale3D'][k] for k in 'XYZ']))
    if b['parent']>=0:
        m=bind[b['parent']]@m;v=posed[b['parent']]@v
    bind.append(m);posed.append(v)
original={}
for vertex,bone,weight in source['influences']:
    original.setdefault(vertex,[]).append((bone,weight))
skirt={}
for vertex,bone,weight in trial['influences']:
    if source['bones'][bone]['name'].startswith('CSS_Cloth_Skirt_'):
        skirt.setdefault(bone,[]).append((vertex,weight))
assert len(skirt)==12
deform=[np.asarray(v@b.inverted()) for v,b in zip(posed,bind)]
corrected=list(posed)
drivers={}
for bone,rows in skirt.items():
    contributions={}
    for vertex,influence in rows:
        for driver,weight in original[vertex]:
            contributions[driver]=contributions.get(driver,0.)+influence*weight
    total=sum(contributions.values())
    contributions={j:w/total for j,w in contributions.items()}
    blend=sum(deform[j]*w for j,w in contributions.items())
    # Remove blend-induced scale/shear without moving the weighted pivot.
    pivot=np.asarray(bind[bone].translation)
    target=blend[:3,:3]@pivot+blend[:3,3]
    u,_,vt=np.linalg.svd(blend[:3,:3])
    rotation=u@np.diag([1,1,np.linalg.det(u@vt)])@vt
    rigid=np.eye(4);rigid[:3,:3]=rotation;rigid[:3,3]=target-rotation@pivot
    corrected[bone]=Matrix(rigid.tolist())@bind[bone]
    drivers[source['bones'][bone]['name']]={source['bones'][j]['name']:w for j,w in contributions.items()}

def skin(mesh,transforms):
    result=np.zeros((len(mesh['points']),3))
    points=np.asarray(mesh['points'])
    for vertex,bone,weight in mesh['influences']:
        m=transforms[bone]
        result[vertex]+=weight*(m[:3,:3]@points[vertex]+m[:3,3])
    return result

expected=skin(source,deform)
before=skin(trial,deform)
after=skin(trial,[np.asarray(v@b.inverted()) for v,b in zip(corrected,bind)])
vertices=sorted({v for rows in skirt.values() for v,_ in rows})
def stats(actual):
    error=np.linalg.norm(actual[vertices]-expected[vertices],axis=1)
    return {'mean_cm':float(np.mean(error)),'p95_cm':float(np.percentile(error,95)),'max_cm':float(np.max(error))}

# Preserve non-skirt component transforms, including the unused chain children.
frame=copy.deepcopy(record)
result=frame['pose']['Snapshot']
by_name={b['name']:i for i,b in enumerate(source['bones'])}
for name,entry in zip(result['BoneNames'],result['LocalTransforms'],strict=True):
    if name not in by_name:continue
    index=by_name[name];parent=source['bones'][index]['parent']
    m=corrected[parent].inverted()@corrected[index] if parent>=0 else corrected[index]
    position,rotation,scale=m.decompose()
    entry['Translation']=dict(zip('XYZ',position))
    entry['Rotation']=dict(zip('XYZW',[rotation.x,rotation.y,rotation.z,rotation.w]))
    entry['Scale3D']=dict(zip('XYZ',scale))
receipt={'source_sha256':hashlib.sha256((work/'holiday-hip-clean.mesh.json').read_bytes()).hexdigest(),
    'motion':str(a.motion),'frame':a.frame,'drivers':drivers,'before':stats(before),'after':stats(after),
    'scope':'Offline animation-follow approximation. No secondary dynamics. Error compares bind geometry, without morph displacement. Render uses recorded morphs.'}
output.write_text(json.dumps(receipt,indent=2)+'\n')
(work/f'{a.name}-driver-pose.json').write_text(json.dumps({'frames':[frame],'scope':receipt['scope']},separators=(',',':')))
print(json.dumps({k:receipt[k] for k in ('before','after','scope')},indent=2))

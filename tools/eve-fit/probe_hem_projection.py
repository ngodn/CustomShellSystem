"""Test bounded hem-control contact correction on the measured worst sprint pose."""
import copy
import json
import argparse
import sys
from pathlib import Path
import numpy as np
from mathutils import Matrix,Quaternion,Vector
from mathutils.bvhtree import BVHTree

work=Path(__file__).resolve().parents[2]/'work/eve26'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--candidate',choices=('hem','hem2'),default='hem')
p.add_argument('--radial',action='store_true',help='Constrain regional correction to outward directions')
p.add_argument('--independent',action='store_true',help='Allow each carrier its own bounded correction')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert not (a.independent and a.radial)
label=a.candidate+('-independent' if a.independent else '-radial' if a.radial else '-contact')
output=work/f'{label}-pose.json';assert not output.exists()
mesh=json.loads((work/f'holiday-{a.candidate}.mesh.json').read_text())
frame=json.loads((work/f'sprint48-{a.candidate}-pose.json').read_text())['frames'][0]
mapping=json.loads((work/f'skirt-{a.candidate}.json').read_text())
names={b['name']:i for i,b in enumerate(mesh['bones'])}
groups={}
for i,s in enumerate('FBLR'):
    entries=mapping['panels'][s]
    for name in ([entries] if isinstance(entries,str) else entries):groups[names[name]]=len(groups) if a.independent else i
count=len(groups) if a.independent else 4
local=dict(zip(frame['pose']['Snapshot']['BoneNames'],frame['pose']['Snapshot']['LocalTransforms'],strict=True))
bind=[];pose=[]
for b in mesh['bones']:
    q=b['rotation'];m=Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
    e=local[b['name']];v=Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),Quaternion([e['Rotation'][k] for k in 'WXYZ']),Vector([e['Scale3D'][k] for k in 'XYZ']))
    if b['parent']>=0:m=bind[b['parent']]@m;v=pose[b['parent']]@v
    bind.append(m);pose.append(v)
points=np.asarray(mesh['points'],dtype=float)
mats=np.asarray([np.asarray(m@b.inverted()) for m,b in zip(pose,bind)])
rows=np.asarray(mesh['influences']);vi=rows[:,0].astype(int);bi=rows[:,1].astype(int);weight=rows[:,2]
xyz=np.zeros_like(points)
np.add.at(xyz,vi,(np.einsum('nij,nj->ni',mats[bi,:3,:3],points[vi])+mats[bi,:3,3])*weight[:,None])
W=np.zeros((len(points),count))
for v,b,w in mesh['influences']:
    if b in groups:W[v,groups[b]]+=w
body_count=json.loads((work/'holiday.mesh.audit.json').read_text())['parts'][0]['faces']
triangles=[]
for f in mesh['faces'][:body_count]:
    ids=[mesh['wedges'][w][0] for w in f[:3]]
    if np.dot(np.cross(points[ids[1]]-points[ids[0]],points[ids[2]]-points[ids[0]]),np.sum([mesh['normals'][w] for w in f[:3]],axis=0))<0:
        ids[1],ids[2]=ids[2],ids[1]
    triangles.append(ids)
tree=BVHTree.FromPolygons(xyz.tolist(),triangles,all_triangles=True)
slot=mesh['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
vertices=sorted({mesh['wedges'][w][0] for f in mesh['faces'] if f[3]==slot for w in f[:3] if points[mesh['wedges'][w][0],2]<120})
shifts=np.zeros((count,3));history=[]
pelvis=names['pelvis'];basis=pose[pelvis]@bind[pelvis].inverted()
directions=np.asarray([list((basis.to_3x3()@Vector(d)).normalized()) for d in [(0,-1,0),(0,1,0),(1,0,0),(-1,0,0)]])
amplitudes=np.zeros(4)
def contacts():
    hits=[]
    for v in vertices:
        position=Vector(xyz[v]+W[v]@shifts)
        nearest,normal,_,_=tree.find_nearest(position)
        signed=(position-nearest).dot(normal)
        if signed<.025:hits.append((signed,v,np.asarray(normal)))
    return sorted(hits,key=lambda row:row[0])
for iteration in range(20):
    hits=contacts()
    history.append({'iteration':iteration,'min_signed_cm':hits[0][0] if hits else .025,'over_1mm':sum(s<-.1 for s,_,_ in hits)})
    if not hits:break
    for _,v,_ in hits:
        position=Vector(xyz[v]+W[v]@shifts)
        nearest,normal,_,_=tree.find_nearest(position)
        signed=(position-nearest).dot(normal)
        if signed>=.025:continue
        if a.radial:
            options=[]
            for j in range(4):
                if W[v,j]<.025:continue
                direction=Vector(directions[j])
                hit,n,_,distance=tree.ray_cast(position,direction,500)
                if hit is None or n.dot(direction)<=0:continue
                correction=(distance+.025)/W[v,j]
                if correction<=8-amplitudes[j]:options.append((correction,j))
            if options:
                correction,j=min(options);amplitudes[j]+=correction
                shifts=amplitudes[:,None]*directions
            continue
        gradient=W[v,:,None]*np.asarray(normal)[None,:]
        denominator=float(np.sum(gradient**2))
        if denominator<.001:continue
        shifts+=gradient*(.025-signed)/denominator*.35
        lengths=np.linalg.norm(shifts,axis=1)
        shifts*=np.minimum(1.,8./np.maximum(lengths,1e-9))[:,None]
hits=contacts()
candidate=list(pose)
for bone,group in groups.items():
    shift=shifts[group]
    candidate[bone]=pose[bone].copy();candidate[bone].translation+=Vector(shift)
result=copy.deepcopy(frame)
lookup=dict(zip(result['pose']['Snapshot']['BoneNames'],result['pose']['Snapshot']['LocalTransforms'],strict=True))
for i,b in enumerate(mesh['bones']):
    parent=b['parent'];m=candidate[parent].inverted()@candidate[i] if parent>=0 else candidate[i]
    t,q,s=m.decompose();e=lookup[b['name']]
    e['Translation']=dict(zip('XYZ',t));e['Rotation']=dict(zip('XYZW',[q.x,q.y,q.z,q.w]));e['Scale3D']=dict(zip('XYZ',s))
scope='Offline static contact projection on hem controls, capped at 8 cm. Not a temporal cloth solver, not runtime collision, and not accepted fitting.'
output.write_text(json.dumps({'scope':scope,'frames':[result]},separators=(',',':')))
report={'scope':scope,'radial':a.radial,'independent':a.independent,'iterations':history,'final_min_signed_cm':hits[0][0] if hits else .025,
    'final_over_1mm':sum(s<-.1 for s,_,_ in hits),
    'shifts_cm':dict(zip([mesh['bones'][b]['name'] for b in groups] if a.independent else 'FBLR',shifts.tolist())),
    'max_vertex_shift_cm':float(np.linalg.norm(W@shifts,axis=1).max()),
    'remaining_samples':[{'vertex':v,'signed_cm':s,'control_weight':W[v].tolist()} for s,v,_ in hits[:12]]}
(work/f'{label}.json').write_text(json.dumps(report,indent=2)+'\n')
print({k:report[k] for k in ('final_min_signed_cm','final_over_1mm','max_vertex_shift_cm','shifts_cm')})

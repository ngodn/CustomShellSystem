"""Fit lower-torso garment vertices against simultaneous sampled body planes."""
import copy,hashlib,json
from pathlib import Path
import numpy as np
from mathutils import Matrix,Quaternion,Vector
from mathutils.bvhtree import BVHTree
w=Path(__file__).resolve().parents[2]/'work/eve26'
p=w/'skin-weight2/skin.mesh.json';data=json.loads(p.read_text());audit=json.loads(p.with_suffix('.audit.json').read_text())
assert hashlib.sha256(p.read_bytes()).hexdigest()==audit['output_sha256']
base=np.asarray(data['points']);nb=audit['parts'][0]['points'];ns=audit['parts'][1]['points']
faces=[[data['wedges'][i][0] for i in f[:3]] for f in data['faces'][:audit['parts'][0]['faces']]]
selected=[i for i in range(nb,nb+ns) if 75<base[i,2]<110]
bind=[]
for b in data['bones']:
 q=b['rotation'];m=Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
 bind.append(bind[b['parent']]@m if b['parent']>=0 else m)
inf=np.asarray(data['influences']);vi,bi,wt=inf[:,0].astype(int),inf[:,1].astype(int),inf[:,2]
morph=np.zeros_like(base)
for target in data['morph_targets']:
 if target['name'] in ('FBMBodyTone','PBMBreastsSize','PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth'):
  for i,*v in target['deltas']:morph[i]+=v
motion=json.loads((w/'planet-f13-sprint.json').read_text())
constraints={i:[] for i in selected};cases=[];reflect=np.array([1,-1,1])
for frame in (-1,8,32,48):
 pose=[]
 if frame==-1:pose=bind
 else:
  snap=motion['frames'][frame]['pose']['Snapshot'];entries=dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True))
  for b in data['bones']:
   t=entries[b['name']];m=Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
   pose.append(pose[b['parent']]@m if b['parent']>=0 else m)
 matrices=np.asarray([np.asarray(a@b.inverted()) for a,b in zip(pose,bind)])
 rotations=np.zeros((len(base),3,3));np.add.at(rotations,vi,matrices[bi,:3,:3]*wt[:,None,None])
 for maximum in (False,True):
  points=base+(morph if maximum else 0);posed=np.zeros_like(base)
  np.add.at(posed,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],points[vi])+matrices[bi,:3,3])*wt[:,None])
  tree=BVHTree.FromPolygons([Vector(v) for v in posed[:nb]*reflect],faces,all_triangles=True)
  negative=0
  for i in selected:
   point=Vector(posed[i]*reflect);hit,normal,_,distance=tree.find_nearest(point)
   if distance>2:continue
   signed=(point-hit).dot(normal);negative+=signed<0
   a=(np.asarray(normal)*reflect)@rotations[i]
   rhs=.06-signed
   constraints[i].append((a,rhs))
  cases.append(dict(frame=frame,maximum_morphs=maximum,inside_vertices=int(negative)))
result=copy.deepcopy(data);offsets=[];conflicts=[]
for i,rows in constraints.items():
 if not rows:continue
 a=np.asarray([r[0] for r in rows]);b=np.asarray([r[1] for r in rows])
 if np.all(b<=0):continue
 x=np.zeros(3);corrections=np.zeros((len(rows)+1,3))
 for iteration in range(150):
  previous=x.copy()
  for k,(n,rhs) in enumerate(rows):
   y=x+corrections[k];length=n@n
   x=y+n*max(0,(rhs-n@y)/length) if length>1e-12 else y
   corrections[k]=y-x
  y=x+corrections[-1];length=np.linalg.norm(y)
  x=y*min(1,.4/max(length,1e-12));corrections[-1]=y-x
  if np.linalg.norm(x-previous)<1e-8 and np.min(a@x-b)>=-1e-6:break
 residual=float(max(0,np.max(b-a@x)))
 if residual>1e-5:conflicts.append(dict(vertex=i,residual_cm=residual));continue
 if np.linalg.norm(x)<1e-7:continue
 result['points'][i]=(base[i]+x).tolist();offsets.append([i,*x.tolist()])
assert result['points'][:nb]==data['points'][:nb] and result['points'][nb+ns:]==data['points'][nb+ns:]
assert all(result[k]==v for k,v in data.items() if k!='points')
out=w/'skin-fit10';out.mkdir(exist_ok=False)
path=out/'skin.mesh.json';path.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
(out/'skin.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'offsets.json').write_text(json.dumps(dict(offsets=offsets))+'\n')
(out/'receipt.json').write_text(json.dumps(dict(cases=cases,changed_vertices=len(offsets),conflicts=conflicts,
    scope='Simultaneous linearized vertex constraints, 0.4 cm bound. Not triangle-interior or full nonlinear collision acceptance.'),indent=2)+'\n')
print('Changed:',len(offsets),'Conflicts:',len(conflicts),flush=True)

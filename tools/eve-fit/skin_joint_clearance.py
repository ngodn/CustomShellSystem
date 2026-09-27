"""Fit lower-torso garment vertices against simultaneous sampled body planes."""
import argparse,copy,hashlib,json,sys
from pathlib import Path
import numpy as np
from mathutils import Matrix,Quaternion,Vector
from mathutils.bvhtree import BVHTree
w=Path(__file__).resolve().parents[2]/'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesh',type=Path,default=w/'skin-fit10/skin.mesh.json')
parser.add_argument('--output',type=Path,default=w/'skin-fit11')
parser.add_argument('--min-z',type=float,default=75)
parser.add_argument('--max-z',type=float,default=110)
parser.add_argument('--max-offset-cm',type=float,default=.4)
parser.add_argument('--coupled',action='store_true')
parser.add_argument('--surface-grid',type=int,default=0)
parser.add_argument('--vertices',type=Path)
parser.add_argument('--iterations',type=int,default=150)
parser.add_argument('--frames',type=int,nargs='+',default=[8,32,48])
parser.add_argument('--motion',type=Path,default=w/'planet-f13-sprint.json')
parser.add_argument('--upstream',action='store_true',help='Use recorded upstream transforms rather than post-process output')
parser.add_argument('--stem',choices=('skin','knit','alice','aegis'),default='skin')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert 0 < args.max_offset_cm <= 1.0
assert args.min_z<args.max_z and not args.output.exists()
assert 0<=args.surface_grid<=16
p=args.mesh;data=json.loads(p.read_text());audit=json.loads(p.with_suffix('.audit.json').read_text())
assert hashlib.sha256(p.read_bytes()).hexdigest()==audit['output_sha256']
base=np.asarray(data['points']);nb=audit['parts'][0]['points'];ns=audit['parts'][1]['points']
faces=[[data['wedges'][i][0] for i in f[:3]] for f in data['faces'][:audit['parts'][0]['faces']]]
selected=[i for i in range(nb,nb+ns) if args.min_z<base[i,2]<args.max_z]
if args.vertices:
 allowed=set(json.loads(args.vertices.read_text()));selected=[i for i in selected if i in allowed]
selected_set=set(selected)
assert selected and args.iterations>0
garment_faces=[[data['wedges'][i][0] for i in f[:3]] for f in data['faces'][audit['parts'][0]['faces']:audit['parts'][0]['faces']+audit['parts'][1]['faces']]]
interior_faces=[ids for ids in garment_faces if all(i in selected_set for i in ids)]
bind=[]
for b in data['bones']:
 q=b['rotation'];m=Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
 bind.append(bind[b['parent']]@m if b['parent']>=0 else m)
inf=np.asarray(data['influences']);vi,bi,wt=inf[:,0].astype(int),inf[:,1].astype(int),inf[:,2]
morph=np.zeros_like(base)
for target in data['morph_targets']:
 if target['name'] in ('FBMBodyTone','PBMBreastsSize','PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth'):
  for i,*v in target['deltas']:morph[i]+=v
motion=json.loads(args.motion.read_text())
constraints={i:[] for i in selected};surface_constraints=[];cases=[];reflect=np.array([1,-1,1])
for frame in [-1,*args.frames]:
 pose=[]
 if frame==-1:pose=bind
 else:
  row=motion['frames'][frame]
  snap=row['upstream'] if args.upstream else row['pose']['Snapshot']
  entries=dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True))
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
  interior=0
  for ids in interior_faces:
   a,b,c=[Vector(posed[i]*reflect) for i in ids]
   samples=[((a+b+c)/3,[1/3]*3)]
   if args.surface_grid:
    n=args.surface_grid
    samples.extend(((a*x+b*y+c*(n-x-y))/n,[x/n,y/n,(n-x-y)/n]) for x in range(n+1) for y in range(n+1-x))
   for point,bary in samples:
    hit,normal,_,distance=tree.find_nearest(point)
    if distance>.5 or (b-a).cross(c-a).normalized().dot(normal)<.5:continue
    signed=(point-hit).dot(normal)
    if signed>=.06:continue
    interior+=1
    normals=[(np.asarray(normal)*reflect)@rotations[i] for i in ids]
    if args.coupled:surface_constraints.append((ids,bary,normals,.06-signed))
    else:
     for i,n in zip(ids,normals):constraints[i].append((n,.06-signed))
  cases.append(dict(frame=frame,maximum_morphs=maximum,inside_vertices=int(negative),interior_constraints=interior))
result=copy.deepcopy(data);offsets=[];conflicts=[]
if args.coupled:
 assert len(selected)<=128,'Coupled solver is for local patches'
 index={v:i for i,v in enumerate(selected)};rows=[];rhs=[]
 for v,items in constraints.items():
  for normal,b in items:
   row=np.zeros((len(selected),3));row[index[v]]=normal;rows.append(row.ravel());rhs.append(b)
 for ids,bary,normals,b in surface_constraints:
  row=np.zeros((len(selected),3))
  for v,t,n in zip(ids,bary,normals):row[index[v]]+=t*n
  rows.append(row.ravel());rhs.append(b)
 a=np.asarray(rows);b=np.asarray(rhs);x=np.zeros(len(selected)*3);corrections=np.zeros((len(rows)+1,len(x)))
 for iteration in range(args.iterations):
  previous=x.copy()
  for k,(n,rhs) in enumerate(zip(a,b)):
   y=x+corrections[k];length=n@n
   x=y+n*max(0,(rhs-n@y)/length) if length>1e-12 else y
   corrections[k]=y-x
  y=x+corrections[-1];blocks=y.reshape(-1,3);lengths=np.linalg.norm(blocks,axis=1)
  x=(blocks*np.minimum(1,args.max_offset_cm/np.maximum(lengths,1e-12))[:,None]).ravel();corrections[-1]=y-x
  if np.linalg.norm(x-previous)<1e-8 and np.min(a@x-b)>=-1e-6:break
 residual=float(max(0,np.max(b-a@x)))
 if residual>1e-5:conflicts.append(dict(patch=selected,residual_cm=residual))
 else:
  for i,delta in zip(selected,x.reshape(-1,3)):
   if np.linalg.norm(delta)<1e-7:continue
   result['points'][i]=(base[i]+delta).tolist();offsets.append([i,*delta.tolist()])
else:
 for i,rows in constraints.items():
  if not rows:continue
  a=np.asarray([r[0] for r in rows]);b=np.asarray([r[1] for r in rows])
  if np.all(b<=0):continue
  x=np.zeros(3);corrections=np.zeros((len(rows)+1,3))
  for iteration in range(args.iterations):
   previous=x.copy()
   for k,(n,rhs) in enumerate(rows):
    y=x+corrections[k];length=n@n
    x=y+n*max(0,(rhs-n@y)/length) if length>1e-12 else y
    corrections[k]=y-x
   y=x+corrections[-1];length=np.linalg.norm(y)
   x=y*min(1,args.max_offset_cm/max(length,1e-12));corrections[-1]=y-x
   if np.linalg.norm(x-previous)<1e-8 and np.min(a@x-b)>=-1e-6:break
  residual=float(max(0,np.max(b-a@x)))
  if residual>1e-5:conflicts.append(dict(vertex=i,residual_cm=residual));continue
  if np.linalg.norm(x)<1e-7:continue
  result['points'][i]=(base[i]+x).tolist();offsets.append([i,*x.tolist()])
assert result['points'][:nb]==data['points'][:nb] and result['points'][nb+ns:]==data['points'][nb+ns:]
assert all(result[k]==v for k,v in data.items() if k!='points')
out=args.output;out.mkdir(exist_ok=False)
path=out/f'{args.stem}.mesh.json';path.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
(out/f'{args.stem}.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'offsets.json').write_text(json.dumps(dict(offsets=offsets))+'\n')
(out/'receipt.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),motion=str(args.motion),motion_sha256=hashlib.sha256(args.motion.read_bytes()).hexdigest(),upstream=args.upstream,region_z_cm=[args.min_z,args.max_z],max_offset_cm=args.max_offset_cm,selected_vertices=len(selected),iterations=args.iterations,surface_grid=args.surface_grid,coupled=args.coupled,cases=cases,changed_vertices=len(offsets),conflicts=conflicts,
    scope='Simultaneous linearized vertex and centroid constraints, Configured displacement bound. Unresolved vertices retain original positions. Requires nonlinear and visible collision verification.'),indent=2)+'\n')
print('Changed:',len(offsets),'Conflicts:',len(conflicts),flush=True)

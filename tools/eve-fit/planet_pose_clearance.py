"""Propose bounded suit offsets for measured triangle-interior sprint intersections."""
import argparse,copy,hashlib,json,sys
from pathlib import Path
import numpy as np
from mathutils import Matrix,Quaternion,Vector
from mathutils.bvhtree import BVHTree
w=Path(__file__).resolve().parents[2]/'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesh',type=Path,default=w/'planet-weight-repair/planet.mesh.json')
parser.add_argument('--motion',type=Path,default=w/'holiday-sprint-motion.json')
parser.add_argument('--frames',type=int,nargs='+',default=[8,16,24])
parser.add_argument('--interior-margin',type=float,default=.03)
parser.add_argument('--centroids',action='store_true')
parser.add_argument('--min-z',type=float,default=22.)
parser.add_argument('--max-z',type=float,default=1000.)
parser.add_argument('--output',type=Path,default=w/'planet-fit7')
parser.add_argument('--garment',choices=('prototype','skin'),default='prototype')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
p=args.mesh;raw=p.read_bytes();m=json.loads(raw)
audit=json.loads(p.with_suffix('.audit.json').read_text());assert hashlib.sha256(raw).hexdigest()==audit['output_sha256']
assert audit['parts'][1]['name']=={'prototype':'Eve Prototype Planet Diving Suit - Suit','skin':'Eve Skin Suit - Suit Complete'}[args.garment]
nb=audit['parts'][0]['points'];ns=audit['parts'][1]['points'];nf=audit['parts'][0]['faces']
base=np.asarray(m['points']);faces=[[m['wedges'][j][0] for j in f[:3]] for f in m['faces'][nf:nf+audit['parts'][1]['faces']]]
reflect=np.array([1,-1,1]);tree=BVHTree.FromPolygons([Vector(v) for v in base*reflect],faces,all_triangles=True)
mask=json.loads((w/'planet-body-mask.json').read_text()) if args.garment=='prototype' else {}
hidden=set(mask.get('hidden_vertices',[]))
samples=[i for i in range(nb) if i not in hidden and base[i,2]>22 and tree.find_nearest(Vector(base[i]*reflect))[3]<.7]
centroids=[]
if args.centroids:
 hidden_faces=set(mask.get('hidden_body_faces',[]))
 for index,face in enumerate(m['faces'][:nf]):
  if index in hidden_faces:continue
  ids=[m['wedges'][j][0] for j in face[:3]];point=base[ids].mean(axis=0)
  if point[2]>22 and tree.find_nearest(Vector(point*reflect))[3]<.7:centroids.append(ids)
inf=np.asarray(m['influences']);vi,bi,wt=inf[:,0].astype(int),inf[:,1].astype(int),inf[:,2]
bind=[]
for b in m['bones']:
 q=b['rotation'];local=Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
 bind.append(bind[b['parent']]@local if b['parent']>=0 else local)
motion=json.loads(args.motion.read_text()) if any(f>=0 for f in args.frames) else None
morph=np.zeros_like(base)
for target in m['morph_targets']:
 if target['name'].startswith(('FBM','PBM')):
  for i,*v in target['deltas']:morph[i]+=v
proposals={};reports=[]
for frame in args.frames:
 pose=[]
 if frame==-1:
  pose=bind
 else:
  row=motion['frames'][frame];snap=row['upstream'] if 'upstream' in row else row['pose']['Snapshot'];entries=dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True))
  for b in m['bones']:
   t=entries[b['name']];local=Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
   pose.append(pose[b['parent']]@local if b['parent']>=0 else local)
 matrices=np.asarray([np.asarray(a@b.inverted()) for a,b in zip(pose,bind)])
 rotations=np.zeros((len(base),3,3));np.add.at(rotations,vi,matrices[bi,:3,:3]*wt[:,None,None])
 for maximum in [False,True]:
  points=base+(morph if maximum else 0);posed=np.zeros_like(points)
  np.add.at(posed,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],points[vi])+matrices[bi,:3,3])*wt[:,None])
  xyz=[Vector(v) for v in posed*reflect];tree=BVHTree.FromPolygons(xyz,faces,all_triangles=True);hits=[];skipped=0
  body_tree=BVHTree.FromPolygons(xyz,[[m['wedges'][j][0] for j in f[:3]] for f in m['faces'][:nf]],all_triangles=True) if args.garment=='skin' else None
  query=[xyz[i] for i in samples]+[sum((xyz[i] for i in ids),Vector())/3 for ids in centroids]
  for i,point in enumerate(query):
   location,normal,face,distance=tree.find_nearest(point);signed=(point-location).dot(normal)
   if body_tree is not None and normal.dot(body_tree.find_nearest(point)[1])<.5:continue
   if not .01<signed<.5 or distance>signed*1.01:continue
   indices=faces[face];a,b,c=[xyz[j] for j in indices];u,v,d=b-a,c-a,location-a
   denom=u.dot(u)*v.dot(v)-u.dot(v)**2
   if denom<1e-10:continue
   beta=(v.dot(v)*d.dot(u)-u.dot(v)*d.dot(v))/denom;gamma=(u.dot(u)*d.dot(v)-u.dot(v)*d.dot(u))/denom
   if min(1-beta-gamma,beta,gamma)<args.interior_margin:continue
   hits.append((i,signed))
   for j in indices:
    if not args.min_z<=base[j,2]<=args.max_z or np.linalg.cond(rotations[j])>10:skipped+=1;continue
    delta=np.linalg.solve(rotations[j],np.asarray(normal)*reflect*(signed+.06))
    if np.linalg.norm(delta)>.4:skipped+=1;continue
    if j not in proposals or np.linalg.norm(delta)>np.linalg.norm(proposals[j]):proposals[j]=delta
  reports.append(dict(frame=frame,maximum_morphs=maximum,hits=len(hits),deepest_cm=max((v for _,v in hits),default=0),skipped_vertex_proposals=skipped))
result=copy.deepcopy(m)
for i,delta in proposals.items():result['points'][i]=(base[i]+delta).tolist()
assert result['points'][:nb]==m['points'][:nb]
assert result['points'][nb+ns:]==m['points'][nb+ns:]
assert all(result[k]==v for k,v in m.items() if k!='points')
out=args.output;out.mkdir(exist_ok=False)
stem='planet' if args.garment=='prototype' else 'skin'
f=out/f'{stem}.mesh.json';f.write_text(json.dumps(result,separators=(',',':'))+'\n');audit['output_sha256']=hashlib.sha256(f.read_bytes()).hexdigest()
(out/f'{stem}.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'offsets.json').write_text(json.dumps({'offsets':[[i,*v.tolist()] for i,v in proposals.items()]})+'\n')
report=dict(cases=reports,body_normal_alignment_minimum=.5 if args.garment=='skin' else None,changed_vertices=len(proposals),max_offset_cm=max((np.linalg.norm(v) for v in proposals.values()),default=0),scope='Bounded pose-derived garment proposal; masked body vertices and non-interior projections excluded. Not watertight collision proof; requires visual review and source application.')
(out/'receipt.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

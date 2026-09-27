"""Locate sleeve penetration in the recorded raised-arm pose without editing assets."""
import argparse,hashlib,json,sys
from pathlib import Path
import numpy as np
from mathutils import Matrix,Quaternion,Vector
from mathutils.bvhtree import BVHTree
w=Path(__file__).resolve().parents[2]/'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesh',type=Path,default=w/'skin-f13-export/skin.mesh.json')
parser.add_argument('--receipt',type=Path,default=w/'skin-fit13/receipt.json')
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
p=args.mesh;data=json.loads(p.read_text())
audit=json.loads(p.with_suffix('.audit.json').read_text())
assert hashlib.sha256(p.read_bytes()).hexdigest()==audit['output_sha256']
base=np.asarray(data['points']);nb=audit['parts'][0]['points'];ns=audit['parts'][1]['points']
points=base.copy()
for target in data['morph_targets']:
 if target['name'] in ('FBMBodyTone','PBMBreastsSize','PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth'):
  for i,*v in target['deltas']:points[i]+=v
snap=json.loads((w/'planet-f13-sprint.json').read_text())['frames'][24]['pose']['Snapshot']
entries=dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True));bind=[];pose=[]
for b in data['bones']:
 q=b['rotation'];m=Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
 bind.append(bind[b['parent']]@m if b['parent']>=0 else m)
 t=entries[b['name']];m=Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
 pose.append(pose[b['parent']]@m if b['parent']>=0 else m)
matrices=np.asarray([np.asarray(a@b.inverted()) for a,b in zip(pose,bind)])
inf=np.asarray(data['influences']);vi,bi,wt=inf[:,0].astype(int),inf[:,1].astype(int),inf[:,2]
posed=np.zeros_like(points)
np.add.at(posed,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],points[vi])+matrices[bi,:3,3])*wt[:,None])
reflect=np.array([1,-1,1]);posed*=reflect
faces=[[data['wedges'][i][0] for i in f[:3]] for f in data['faces'][:audit['parts'][0]['faces']]]
tree=BVHTree.FromPolygons([Vector(v) for v in posed[:nb]],faces,all_triangles=True)
arms={i for i,b in enumerate(data['bones']) if b['name'].startswith('upperarm')}
arm_weight=np.zeros(len(base));np.add.at(arm_weight,vi[np.isin(bi,list(arms))],wt[np.isin(bi,list(arms))])
conflicts={r['vertex']:r['residual_cm'] for r in json.loads(args.receipt.read_text())['conflicts']}
rows=[]
for i in range(nb,nb+ns):
 if arm_weight[i]<.5:continue
 point=Vector(posed[i]);hit,normal,face,distance=tree.find_nearest(point)
 signed=(point-hit).dot(normal)
 if signed>=0:continue
 rows.append(dict(vertex=i,signed_cm=signed,rest=base[i].tolist(),posed_cm=posed[i].tolist(),arm_weight=arm_weight[i],conflict_residual=conflicts.get(i)))
rows.sort(key=lambda r:r['signed_cm'])
interiors=[]
start=audit['parts'][0]['faces']
for face_index,record in enumerate(data['faces'][start:start+audit['parts'][1]['faces']],start):
 ids=[data['wedges'][k][0] for k in record[:3]]
 if min(arm_weight[ids])<.5:continue
 a,b,c=[Vector(posed[i]) for i in ids]
 point=(a+b+c)/3;hit,normal,face,distance=tree.find_nearest(point)
 signed=(point-hit).dot(normal)
 if signed>=0 or (b-a).cross(c-a).normalized().dot(normal)<.5:continue
 interiors.append(dict(face=face_index,vertices=ids,signed_cm=signed,rest_center=base[ids].mean(axis=0).tolist(),posed_center=list(point),conflicts=[conflicts.get(i) for i in ids]))
interiors.sort(key=lambda r:r['signed_cm'])
out=args.output;assert not out.exists()
out.write_text(json.dumps(dict(source_sha256=audit['output_sha256'],frame=24,combined_max=True,negative_arm_vertices=rows,negative_arm_centroids=interiors,scope='Nearest-body signed distances on arm-weighted garment vertices, not exhaustive triangle collision.'),indent=2)+'\n')
print(json.dumps(interiors[:15],indent=2))

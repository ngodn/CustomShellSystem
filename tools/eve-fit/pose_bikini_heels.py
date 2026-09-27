"""Measure a rigid heel pose that preserves each shoe's geometry."""
import copy,hashlib,json,math
from pathlib import Path
import numpy as np
from mathutils import Matrix,Quaternion,Vector
w=Path(__file__).resolve().parents[2]/'work/eve26'
p=w/'bikini-weight1/bikini.mesh.json';data=json.loads(p.read_text());audit=json.loads(p.with_suffix('.audit.json').read_text())
assert hashlib.sha256(p.read_bytes()).hexdigest()==audit['output_sha256']
pi=next(i for i,p in enumerate(audit['parts']) if p['name']=='Eve Extras - Heels');start=sum(p['points'] for p in audit['parts'][:pi]);end=start+audit['parts'][pi]['points'];fs=sum(p['faces'] for p in audit['parts'][:pi]);records=data['faces'][fs:fs+audit['parts'][pi]['faces']]
xyz=np.asarray(data['points']);result=copy.deepcopy(data)
sole=data['materials'].index('sole-1');tap=data['materials'].index('tap-1')
sole_ids={data['wedges'][k][0] for f in records if f[3]==sole for k in f[:3]};tap_ids={data['wedges'][k][0] for f in records if f[3]==tap for k in f[:3]}
world=[]
for b in data['bones']:
 q=b['rotation'];m=Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']));world.append(world[b['parent']]@m if b['parent']>=0 else m)
bones={b['name']:m for b,m in zip(data['bones'],world)}
rotations={};rows=[]
for sign in (-1,1):
 ids=np.array([i for i in range(start,end) if xyz[i,0]*sign>0]);front=np.array([i for i in sole_ids if xyz[i,0]*sign>0 and xyz[i,1]>5]);tips=np.array([i for i in tap_ids if xyz[i,0]*sign>0])
 center_front=xyz[front[xyz[front,2]<xyz[front,2].min()+.1]].mean(axis=0);center_tip=xyz[tips[xyz[tips,2]<xyz[tips,2].min()+.1]].mean(axis=0)
 forward=center_front-center_tip;forward[2]=0;forward/=np.linalg.norm(forward);axis=Vector(np.cross([0,0,1],forward))
 bone=min(('foot_l','foot_r'),key=lambda n:np.linalg.norm(np.asarray(bones[n].translation)-xyz[ids].mean(axis=0)))
 pivot=np.asarray(bones[bone].translation)
 def rotate(angle,points):return (np.asarray(Matrix.Rotation(angle,3,axis))@(points-pivot).T).T+pivot
 def gap(angle):return float(rotate(angle,xyz[front])[:,2].min()-rotate(angle,xyz[tips])[:,2].min())
 lo,hi=-math.pi/3,math.pi/3;assert gap(lo)*gap(hi)<0
 for _ in range(60):
  mid=(lo+hi)/2
  if gap(mid)*gap(lo)>0:lo=mid
  else:hi=mid
 angle=(lo+hi)/2;rot=np.asarray(Matrix.Rotation(angle,3,axis));posed=rotate(angle,xyz[ids])
 assert abs(gap(angle))<.0001
 assert np.max(np.abs(np.linalg.norm(posed-pivot,axis=1)-np.linalg.norm(xyz[ids]-pivot,axis=1)))<.00001
 for i,v in zip(ids,posed):result['points'][i]=v.tolist();rotations[int(i)]=rot
 level=float(rotate(angle,xyz[front])[:,2].min())
 rows.append(dict(side=sign,foot_bone=bone,pivot_cm=pivot.tolist(),axis=list(axis),angle_degrees=math.degrees(angle),contact_z_cm=level,offset_to_original_forefoot_cm=float(xyz[front,2].min()-level)))
for face in records:
 for wi in face[:3]:
  vertex=data['wedges'][wi][0];result['normals'][wi]=(rotations[vertex]@np.asarray(data['normals'][wi])).tolist()
assert result['points'][:start]==data['points'][:start] and result['points'][end:]==data['points'][end:]
assert all(result[k]==v for k,v in data.items() if k not in ('points','normals'))
out=w/'bikini-heelpose1';out.mkdir(exist_ok=False);p=out/'bikini.mesh.json';p.write_text(json.dumps(result,separators=(',',':'))+'\n');audit['output_sha256']=hashlib.sha256(p.read_bytes()).hexdigest();p.with_suffix('.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'receipt.json').write_text(json.dumps(dict(shoes=rows,body_unchanged=True,rigid_shoe_shapes_preserved=True,scope='Shoe-only rigid pose candidate. Exposed feet need matching pose/lining; measured offset is not applied. Not game floor acceptance.'),indent=2)+'\n');print(rows,flush=True)

"""Fit the lower heel supports to each forefoot sole without moving the uppers."""
import copy,hashlib,json
from pathlib import Path
import numpy as np
w=Path(__file__).resolve().parents[2]/'work/eve26'
p=w/'bikini-morph1/bikini.mesh.json';data=json.loads(p.read_text());audit=json.loads(p.with_suffix('.audit.json').read_text())
assert hashlib.sha256(p.read_bytes()).hexdigest()==audit['output_sha256']
part_index=next(i for i,part in enumerate(audit['parts']) if part['name']=='Eve Extras - Heels')
begin=sum(part['points'] for part in audit['parts'][:part_index]);end=begin+audit['parts'][part_index]['points']
fbegin=sum(part['faces'] for part in audit['parts'][:part_index]);records=data['faces'][fbegin:fbegin+audit['parts'][part_index]['faces']]
points=np.asarray(data['points']);result=copy.deepcopy(data);changed=[];sides=[]
sole=data['materials'].index('sole-1');tap=data['materials'].index('tap-1')
sole_ids={data['wedges'][k][0] for face in records if face[3]==sole for k in face[:3]}
tap_ids={data['wedges'][k][0] for face in records if face[3]==tap for k in face[:3]}
for sign in (-1,1):
 front=[i for i in sole_ids if points[i,0]*sign>0 and points[i,1]>5]
 tips=[i for i in tap_ids if points[i,0]*sign>0]
 target=min(points[i,2] for i in front);bottom=min(points[i,2] for i in tips)
 assert bottom<target<0
 for i in range(begin,end):
  x,y,z=points[i]
  if x*sign<=0 or z>=0:continue
  blend=np.clip((3-y)/3,0,1);blend=blend*blend*(3-2*blend)
  offset=(target/bottom-1)*z*blend
  if abs(offset)<1e-9:continue
  result['points'][i][2]+=float(offset);changed.append([i,0,0,float(offset)])
 sides.append(dict(side=sign,original_tip_z_cm=bottom,target_forefoot_z_cm=target,compression_ratio=target/bottom))
assert result['points'][:begin]==data['points'][:begin] and result['points'][end:]==data['points'][end:]
assert all(result[k]==v for k,v in data.items() if k!='points')
tri=np.asarray([[data['wedges'][i][0] for i in face[:3]] for face in records]);xyz=np.asarray(result['points'],dtype=np.float32)
cross=np.cross(xyz[tri[:,1]]-xyz[tri[:,0]],xyz[tri[:,2]]-xyz[tri[:,0]])
assert np.all(np.sum(cross*cross,axis=1)>=1e-12),'Repair collapses footwear triangles'
out=w/'bikini-heel1';out.mkdir(exist_ok=False);p=out/'bikini.mesh.json';p.write_text(json.dumps(result,separators=(',',':'))+'\n');audit['output_sha256']=hashlib.sha256(p.read_bytes()).hexdigest();p.with_suffix('.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'offsets.json').write_text(json.dumps(dict(offsets=changed))+'\n')
(out/'receipt.json').write_text(json.dumps(dict(sides=sides,changed_vertices=len(changed),body_uppers_weights_morphs_unchanged=True,scope='Lower shoe support compression only, rear-to-front blend. Requires silhouette, normals, standing and motion validation; not game floor acceptance.'),indent=2)+'\n')
print(sides,flush=True)

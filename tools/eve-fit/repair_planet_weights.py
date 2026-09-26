"""Transfer Prototype suit skin weights from its corresponding body surface."""
import copy,hashlib,json
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parents[2];work=root/'work/eve26'
p=work/'planet-morph-repair/planet.mesh.json';raw=p.read_bytes();source=json.loads(raw)
audit=json.loads(p.with_name('planet.mesh.audit.json').read_text())
assert hashlib.sha256(raw).hexdigest()==audit['output_sha256']
body_count=audit['parts'][0]['points'];suit_count=audit['parts'][1]['points']
points=[Vector((x,-y,z)) for x,y,z in source['points']]
faces=[[source['wedges'][w][0] for w in f[:3]] for f in source['faces'][:audit['parts'][0]['faces']]]
tree=BVHTree.FromPolygons(points[:body_count],faces,all_triangles=True)
correspondence=[]
for i in range(body_count,body_count+suit_count):
 location,normal,face,distance=tree.find_nearest(points[i]);indices=faces[face]
 a,b,c=[points[j] for j in indices];u,v,w=b-a,c-a,location-a
 denom=u.dot(u)*v.dot(v)-u.dot(v)**2
 assert denom>1e-12
 beta=(v.dot(v)*w.dot(u)-u.dot(v)*w.dot(v))/denom
 gamma=(u.dot(u)*w.dot(v)-u.dot(v)*w.dot(u))/denom
 weights=np.maximum([1-beta-gamma,beta,gamma],0);weights/=weights.sum()
 correspondence.append((indices,weights))
result=copy.deepcopy(source)
body_weights=[{} for _ in range(body_count)]
for vertex,bone,weight in source['influences']:
 if vertex<body_count:body_weights[vertex][bone]=weight
changed=set();new=[]
for j,(indices,bary) in enumerate(correspondence,body_count):
 if points[j].z<22:continue
 weights={}
 for i,alpha in zip(indices,bary):
  for bone,weight in body_weights[i].items():weights[bone]=weights.get(bone,0)+weight*float(alpha)
 kept=sorted(((bone,w) for bone,w in weights.items() if w>1e-7),key=lambda item:-item[1])[:8]
 total=sum(w for _,w in kept);assert total>0
 new.extend([j,bone,w/total] for bone,w in kept);changed.add(j)
result['influences']=[row for row in source['influences'] if row[0] not in changed]+new
assert all(result[k]==v for k,v in source.items() if k!='influences')
assert [row for row in result['influences'] if row[0]<body_count]==[row for row in source['influences'] if row[0]<body_count]
out=work/'planet-weight-repair';out.mkdir(exist_ok=False)
f=out/'planet.mesh.json';f.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(f.read_bytes()).hexdigest()
(out/'planet.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
report=dict(changed_suit_vertices=len(changed),max_influences=8,body_unchanged=True,geometry_and_morphs_unchanged=True,scope='Body-corresponding suit skin candidate above 22 cm. Footwear and accessories unchanged. Requires pose and source verification.')
(out/'receipt.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

"""Lift intersecting War Aegis vertices off the unchanged body surface."""
import copy
import hashlib
import json
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

w=Path(__file__).resolve().parents[2]/'work/eve26'
source=w/'aegis-fit1/aegis.mesh.json';out=w/'aegis-fit2'
assert not out.exists()
data=json.loads(source.read_text());audit=json.loads(source.with_suffix('.audit.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest()==audit['output_sha256']
nb=audit['parts'][0]['points'];ng=audit['parts'][1]['points'];nf=audit['parts'][0]['faces']
points=[Vector(p) for p in data['points']]
faces=[[data['wedges'][wi][0] for wi in f[:3]] for f in data['faces'][:nf]]
# Export winding and UE normal conventions differ; use the saved outward normals.
normals=[]
for face in data['faces'][:nf]:
    n=sum((Vector(data['normals'][wi]) for wi in face[:3]),Vector())
    assert n.length>1e-6
    normals.append(n.normalized())
tree=BVHTree.FromPolygons(points[:nb],faces,all_triangles=True)
result=copy.deepcopy(data);offsets=[];deep=[]
for i in range(nb,nb+ng):
    hit,_,face,distance=tree.find_nearest(points[i]);normal=normals[face]
    signed=(points[i]-hit).dot(normal)
    if signed>=.2:continue
    if distance>3:
        if signed<0:deep.append([i,distance])
        continue
    delta=normal*(.2-signed)
    result['points'][i]=list(points[i]+delta);offsets.append([i,*delta])
assert result['points'][:nb]==data['points'][:nb] and result['points'][nb+ng:]==data['points'][nb+ng:]
assert all(result[k]==v for k,v in data.items() if k!='points')
out.mkdir();p=out/'aegis.mesh.json';p.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(p.read_bytes()).hexdigest();(out/'aegis.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'offsets.json').write_text(json.dumps(dict(offsets=offsets))+'\n')
r=dict(changed_points=len(offsets),max_offset_cm=max(np.linalg.norm(row[1:]) for row in offsets),unresolved_deep=deep,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),body_unchanged=True,scope='Local nearest-surface 0.2 cm clearance, restricted to points within 3 cm. Preserves loose geometry and all morph deltas; requires face-interior, pose and visual checks.')
(out/'receipt.json').write_text(json.dumps(r,indent=2)+'\n');print('changed',len(offsets),'deep',len(deep),flush=True)

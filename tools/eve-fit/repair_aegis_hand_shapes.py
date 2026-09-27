"""Make War Aegis gloves follow the existing left-hand corrective morphs."""
import copy
import hashlib
import json
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

w=Path(__file__).resolve().parents[2]/'work/eve26'
p=w/'aegis-fit3/aegis.mesh.json';out=w/'aegis-hand1'
assert not out.exists()
data=json.loads(p.read_text());audit=json.loads(p.with_suffix('.audit.json').read_text())
assert hashlib.sha256(p.read_bytes()).hexdigest()==audit['output_sha256']
nb=audit['parts'][0]['points'];ng=audit['parts'][1]['points']
points=[Vector(p) for p in data['points']]
faces=[[data['wedges'][i][0] for i in f[:3]] for f in data['faces'][:audit['parts'][0]['faces']]]
tree=BVHTree.FromPolygons(points[:nb],faces,all_triangles=True)
correspondence=[]
for i in range(nb,nb+ng):
    hit,_,fi,distance=tree.find_nearest(points[i]);ids=faces[fi]
    b=barycentric_transform(hit,*(points[j] for j in ids),Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
    weights=np.maximum(list(b),0);weights/=weights.sum()
    correspondence.append((ids,weights))
result=copy.deepcopy(data);rows=[]
for target in result['morph_targets']:
    if not target['name'].startswith('pJCM') or not target['name'].endswith('_L'):continue
    body=np.zeros((nb,3))
    for i,*delta in target['deltas']:
        if i<nb:body[i]=delta
    assert np.count_nonzero(body)>0
    new=[]
    for j,(ids,weights) in enumerate(correspondence):
        delta=sum((body[i]*v for i,v in zip(ids,weights)),np.zeros(3))
        if np.linalg.norm(delta)>1e-5:new.append([nb+j,*delta.tolist()])
    target['deltas']=[r for r in target['deltas'] if not nb<=r[0]<nb+ng]+new
    rows.append(dict(name=target['name'],garment_points=len(new)))
assert len(rows)==16
assert all(result[k]==v for k,v in data.items() if k!='morph_targets')
for old,new in zip(data['morph_targets'],result['morph_targets']):
    assert [r for r in old['deltas'] if r[0]<nb]==[r for r in new['deltas'] if r[0]<nb]
out.mkdir();dest=out/'aegis.mesh.json';dest.write_text(json.dumps(result,separators=(',',':'))+'\n');audit['output_sha256']=hashlib.sha256(dest.read_bytes()).hexdigest()
(out/'aegis.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'receipt.json').write_text(json.dumps(dict(shapes=rows,base_and_body_unchanged=True,source_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='Body-corresponding glove corrective deltas. Does not fix right-hand clipping or prove live corrective activation.'),indent=2)+'\n');print(rows)

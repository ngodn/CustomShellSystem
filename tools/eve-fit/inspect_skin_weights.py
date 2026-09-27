"""Compare lower-torso suit weights with nearby body triangles, without editing."""
import hashlib,json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform
w=Path(__file__).resolve().parents[2]/'work/eve26'
p=w/'skin-lining4/skin.mesh.json';d=json.loads(p.read_text());a=json.loads(p.with_suffix('.audit.json').read_text())
assert hashlib.sha256(p.read_bytes()).hexdigest()==a['output_sha256']
n=a['parts'][0]['points'];s=a['parts'][1]['points']
points=[Vector(p) for p in d['points']]
faces=[[d['wedges'][i][0] for i in f[:3]] for f in d['faces'][:a['parts'][0]['faces']]]
tree=BVHTree.FromPolygons(points[:n],faces,all_triangles=True)
weights={}
for v,b,value in d['influences']:weights.setdefault(v,{})[b]=value
rows=[]
for v in range(n,n+s):
    point=points[v]
    if not (75<point.z<115 and abs(point.x)<20):continue
    hit,_,face,distance=tree.find_nearest(point)
    ids=faces[face]
    bary=barycentric_transform(hit,*[points[i] for i in ids],Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
    target={}
    for i,factor in zip(ids,bary):
        for b,value in weights[i].items():target[b]=target.get(b,0)+value*factor
    current=weights[v]
    difference=sum(abs(current.get(b,0)-target.get(b,0)) for b in current.keys()|target.keys())
    def named(values):return {d['bones'][b]['name']:round(float(value),6) for b,value in values.items() if value>1e-5}
    rows.append(dict(vertex=v,position_cm=list(point),distance_cm=distance,l1_difference=difference,
                     garment=named(current),body=named(target)))
rows.sort(key=lambda r:r['l1_difference'],reverse=True)
out=w/'skin-groin-weights.json';assert not out.exists()
out.write_text(json.dumps(dict(source_sha256=a['output_sha256'],vertices=len(rows),
    difference_above_point2=sum(r['l1_difference']>.2 for r in rows),worst=rows[:30],
    scope='Nearest body weight comparison only. Differences are not automatically defects.'),indent=2)+'\n')
print('Vertices:',len(rows),'L1 difference >0.2:',sum(r['l1_difference']>.2 for r in rows))

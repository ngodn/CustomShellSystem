"""Place trial simulation centers on the fitted dress instead of inside the body."""
import json
import math
from pathlib import Path
from mathutils import Matrix,Quaternion,Vector
from mathutils.bvhtree import BVHTree

work=Path(__file__).resolve().parents[2]/'work/eve26'
mesh=json.loads((work/'holiday-hip-bones.mesh.json').read_text())
recipe=json.loads((work/'holiday-dynamics.json').read_text())
points=[Vector(p) for p in mesh['points']]
slot=mesh['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
faces=[f for f in mesh['faces'] if f[3]==slot]
triangles=[[mesh['wedges'][w][0] for w in f[:3]] for f in faces]
tree=BVHTree.FromPolygons(points,triangles,all_triangles=True)
vertices={v for f in triangles for v in f}
world=[];indices={}
for i,b in enumerate(mesh['bones']):
    q=b['rotation']
    m=Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
    if b['parent']>=0:m=world[b['parent']]@m
    world.append(m);indices[b['name']]=i
influences={}
for vertex,bone,weight in mesh['influences']:
    if vertex in vertices:influences.setdefault(bone,[]).append((vertex,weight))
report=[]
for chain in recipe['chains']:
    chain['angular_spring']=0
    for body in chain['bodies']:
        index=indices[body['bone']]
        rows=influences[index]
        total=sum(w for _,w in rows)
        centroid=sum((points[v]*w for v,w in rows),Vector())/total
        surface,_,face,_=tree.find_nearest(centroid)
        normal=sum((Vector(mesh['normals'][w]) for w in faces[face][:3]),Vector()).normalized()
        center=surface+normal*.35
        old=world[index]@Vector(body['joint_offset'])
        body['joint_offset']=list(world[index].inverted()@center)
        body['box_extents']=[math.sqrt(6)]*3
        body['collision_radius']=.25
        assert max(abs(v) for v in body['joint_offset'])<30
        def clearance(point,radius):
            return min((point-world[indices[s['bone']]]@Vector(s['offset'])).length-s['radius']-radius for s in chain['spheres'])
        report.append({'bone':body['bone'],'old_center_cm':list(old),'center_cm':list(center),
            'old_clearance_cm':clearance(old,.5),'clearance_cm':clearance(center,.25),
            'source_weight_sum':total,'joint_offset_cm':body['joint_offset']})
recipe['scope']='Geometry-derived simulation-center trial. Body/garment geometry unchanged. Requires actual motion and morph/collision validation.'
out=work/'holiday-dynamics2.json';assert not out.exists()
out.write_text(json.dumps(recipe,indent=2)+'\n')
(work/'skirt-body-centers.json').write_text(json.dumps(report,indent=2)+'\n')
print([(r['bone'],r['old_clearance_cm'],r['clearance_cm']) for r in report])

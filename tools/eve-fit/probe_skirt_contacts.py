"""Measure initial simulated body spheres against the configured body collision spheres."""
import json
from pathlib import Path
from mathutils import Matrix,Quaternion,Vector

work=Path(__file__).resolve().parents[2]/'work/eve26'
mesh=json.loads((work/'holiday-hip-bones.mesh.json').read_text())
motion=json.loads((work/'motion-cube-walk-no-spring.json').read_text())
snapshot=motion['frames'][0]['pose']['Snapshot']
local=dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
world=[];by_name={}
for bone in mesh['bones']:
    t=local[bone['name']]
    m=Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
    if bone['parent']>=0:m=world[bone['parent']]@m
    world.append(m);by_name[bone['name']]=m
recipe=json.loads((work/'holiday-dynamics.json').read_text())
report=[]
for chain in recipe['chains']:
    for body in chain['bodies']:
        center=by_name[body['bone']]@Vector(body['joint_offset'])
        intersections=[]
        for sphere in chain['spheres']:
            target=by_name[sphere['bone']]@Vector(sphere['offset'])
            clearance=(target-center).length-sphere['radius']-body['collision_radius']
            if clearance<0:intersections.append({'driver':sphere['bone'],'center_cm':list(target),'radius_cm':sphere['radius'],'depth_cm':-clearance})
        report.append({'bone':body['bone'],'center_cm':list(center),'intersections':intersections})
(work/'skirt-rest-contacts.json').write_text(json.dumps(report,indent=2)+'\n')
print([(r['bone'],max((i['depth_cm'] for i in r['intersections']),default=0)) for r in report])

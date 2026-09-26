"""Compare proxy vertices with saved sphyls using UE's capsule extraction convention."""
import json
import math
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
data = json.loads((WORK/'holiday.mesh.json').read_text())
proxy = json.loads((WORK/'skirt-weights/pelvis-proxy.json').read_text())
physics = json.loads((WORK/'holiday-collision.json').read_text())
anchors = json.loads((WORK/'skirt-anchors.json').read_text())
world = []
for bone in data['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3],*q[:3])), Vector(bone['scale']))
    parent = bone['parent']
    world.append(world[parent] @ local if parent >= 0 else local)
lookup = {b['name']: world[i] for i,b in enumerate(data['bones'])}
points = np.asarray(proxy['positions'])
movable = np.asarray(anchors['max_distances_cm']) > 0
rows = []
union = np.zeros(len(points), dtype=bool)
for body in physics['bodies']:
    name = body['boneName']
    if name not in ('pelvis','thigh_l','thigh_r'):
        continue
    transform = lookup[name]
    assert max(abs(s-1) for s in transform.to_scale()) < .001, 'Nonunit bone scale needs explicit capsule scaling'
    for capsule in body['aggGeom']['sphylElems']:
        r = capsule['rotation']
        pitch,yaw,roll = [math.radians(r[k]) for k in ('pitch','yaw','roll')]
        sp,cp,sy,cy,sr,cr = math.sin(pitch),math.cos(pitch),math.sin(yaw),math.cos(yaw),math.sin(roll),math.cos(roll)
        # UE RotationTranslationMatrix row 2 is rotated UpVector.
        axis = Vector((-(cr*sp*cy+sr*sy), cy*sr-cr*sp*sy, cr*cp))
        center = Vector([capsule['center'][k] for k in ('x','y','z')])
        half = axis*(capsule['length']/2)
        a,b = np.asarray(transform@(center-half)),np.asarray(transform@(center+half))
        segment = b-a
        t = np.clip((points-a)@segment / (segment@segment),0,1)
        distance = np.linalg.norm(points-(a+t[:,None]*segment),axis=1)
        penetration = capsule['radius']-distance
        inside = penetration>0
        union |= inside
        rows.append({'bone':name,'radius_cm':capsule['radius'],'endpoints_cm':[a.tolist(),b.tolist()],
                     'inside_vertices':int(inside.sum()),'inside_movable_vertices':int((inside & movable).sum()),
                     'max_penetration_cm':float(max(0,penetration.max()))})
output = WORK/'skirt-collision-fit.json'
assert not output.exists()
report = {'capsules':rows,'inside_any_movable_vertices':int((union & movable).sum()),
          'movable_vertices':int(movable.sum()),
          'scope':'Rest-pose sphyl containment only; excludes tapered capsules, other bones, animation and collision thickness'}
output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

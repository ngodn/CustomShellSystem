"""Compare native panel samples, deformed body surface and the saved collider recipe."""
import argparse
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--frame', type=int, default=4)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
mesh = json.loads((w/'holiday.mesh.json').read_text())
audit = json.loads((w/'holiday.mesh.audit.json').read_text())
recipe = json.loads((w/'cloth-capsules.json').read_text())
proxy = json.loads((w/'skirt-proxies.json').read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
result = json.loads(a.input.read_text())
motion = json.loads(Path(result['source_motion']).read_text())
snap = motion['frames'][a.frame]['pose']['Snapshot']
entries = dict(zip(snap['BoneNames'], snap['LocalTransforms'], strict=True))
points = np.asarray(mesh['points'])
bind, pose, by_name = [], [], {}
for bone in mesh['bones']:
    q = bone['rotation']
    b = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    e = entries[bone['name']]
    assert max(abs(e['Scale3D'][k]-1) for k in 'XYZ') < 1e-4
    m = Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']), Quaternion([e['Rotation'][k] for k in 'WXYZ']), Vector((1,1,1)))
    parent = bone['parent']
    bind.append(bind[parent]@b if parent >= 0 else b)
    pose.append(pose[parent]@m if parent >= 0 else m)
    by_name[bone['name']] = pose[-1]
mats = np.asarray([np.asarray(m@b.inverted()) for m,b in zip(pose,bind)])
rows = np.asarray(mesh['influences'])
vi,bi,weights = rows[:,0].astype(int),rows[:,1].astype(int),rows[:,2]
xyz = np.zeros_like(points)
np.add.at(xyz, vi, (np.einsum('nij,nj->ni',mats[bi,:3,:3],points[vi])+mats[bi,:3,3])*weights[:,None])
triangles = []
for face in mesh['faces'][:audit['parts'][0]['faces']]:
    ids = [mesh['wedges'][i][0] for i in face[:3]]
    normal = np.sum([mesh['normals'][i] for i in face[:3]],axis=0)
    if np.dot(np.cross(points[ids[1]]-points[ids[0]],points[ids[2]]-points[ids[0]]),normal)<0:
        ids[1],ids[2] = ids[2],ids[1]
    triangles.append(ids)
tree = BVHTree.FromPolygons(xyz.tolist(),triangles,all_triangles=True)
spheres = [(np.asarray(by_name[s['bone']]@Vector(s['local_center_cm'])),s['radius_cm']) for s in recipe['spheres']]
connected = {i for c in recipe['connections'] for i in c['sphere_indices']}

def collider_phi(samples):
    distances = []
    for c in recipe['connections']:
        i,j = c['sphere_indices'];start,r0 = spheres[i];end,r1 = spheres[j]
        axis = end-start;length = np.linalg.norm(axis)
        if length<1e-8 or abs(r1-r0)>=length:
            distances.append(np.minimum(np.linalg.norm(samples-start,axis=1)-r0,np.linalg.norm(samples-end,axis=1)-r1))
            continue
        axis /= length
        axial = (samples-start)@axis
        radial = np.linalg.norm(samples-start-axial[:,None]*axis,axis=1)
        slope = (r1-r0)/length
        along = np.clip(axial+slope*radial/np.sqrt(1-slope*slope),0,length)
        distances.append(np.hypot(axial-along,radial)-r0-slope*along)
    distances.extend(np.linalg.norm(samples-center,axis=1)-radius for i,(center,radius) in enumerate(spheres) if i not in connected)
    return np.min(distances,axis=0)

actual = np.asarray(result['frames'][a.frame]['positions_cm'])
rest = np.asarray(proxy['positions'])
free = rest[:,2] < rest[:,2].max()-20
indices = np.asarray(proxy['indices']).reshape((-1,3))
active_faces = np.any(free[indices],axis=1)
samples = np.concatenate((actual[free],actual[indices[active_faces]].mean(axis=1)))
phi = collider_phi(samples)
signed,body_faces = [],[]
for point in samples:
    nearest,normal,face,_ = tree.find_nearest(Vector(point))
    signed.append((Vector(point)-nearest).dot(normal));body_faces.append(face)
signed = np.asarray(signed)
missed = (signed < -.1) & (phi > .3)
worst = []
for i in np.flatnonzero(missed)[np.argsort(signed[missed])[:12]]:
    counts = []
    for axis in ((1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)):
        origin = Vector(samples[i]);direction = Vector(axis);n = 0
        for _ in range(100):
            hit,_,_,_ = tree.ray_cast(origin,direction,500)
            if hit is None:break
            n += 1;origin = hit+direction*.0001
        counts.append(n)
    worst.append({'position_cm':samples[i].tolist(),'body_signed_cm':float(signed[i]),
        'collider_phi_cm':float(phi[i]),'body_vertices':triangles[body_faces[i]],'ray_counts':counts})
report = {'scope':'Old native panel free vertices and active-face centroids versus preserved deformed body and analytic original PA_Holiday capsule union, even when the input uses a different collider. Signed nearest-surface checks; worst misses include six-ray checks. Not full continuous surface coverage.',
    'input':str(a.input),'frame':a.frame,'samples':len(samples),
    'capsule_reference':str(w/'cloth-capsules.json'),
    'active_physics_asset':result.get('physics_asset_during_run'),
    'body_collision_input':result.get('body_collision_input',''),
    'inside_body_over_1mm':int((signed<-.1).sum()),'inside_body_outside_collision_margin':int(missed.sum()),
    'worst_misses':worst}
a.output.write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k not in ('scope','worst_misses')})
print(worst[:2])

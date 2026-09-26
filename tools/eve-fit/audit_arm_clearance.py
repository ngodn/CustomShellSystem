"""Check traced arm-contact vertices against the unchanged posed torso surface."""
import json
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2]/'work/eve26'
output = work/'arm-torso.json'
assert not output.exists()
mesh = json.loads((work/'holiday-hip-clean.mesh.json').read_text())
motion = json.loads((work/'follow-sprint-base.json').read_text())
trace = json.loads((work/'fabrictrace.json').read_text())
points = np.asarray(mesh['points'], dtype=float)
rows = np.asarray(mesh['influences'])
vi, bi, weights = rows[:, 0].astype(int), rows[:, 1].astype(int), rows[:, 2]
arm_mass, torso_mass = np.zeros(len(points)), np.zeros(len(points))
for v, bone, weight in mesh['influences']:
    name = mesh['bones'][bone]['name']
    if any(s in name for s in ('upperarm', 'lowerarm', 'hand', 'thumb', 'index', 'middle', 'ring', 'pinky')):
        arm_mass[v] += weight
    if name == 'pelvis' or any(s in name for s in ('spine', 'butt', 'belly')):
        torso_mass[v] += weight
body_count = json.loads((work/'holiday.mesh.audit.json').read_text())['parts'][0]['faces']
triangles = []
for face in mesh['faces'][:body_count]:
    ids = [mesh['wedges'][w][0] for w in face[:3]]
    if not all(torso_mass[v] > .5 for v in ids):
        continue
    normal = np.sum([mesh['normals'][w] for w in face[:3]], axis=0)
    if np.dot(np.cross(points[ids[1]]-points[ids[0]], points[ids[2]]-points[ids[0]]), normal) < 0:
        ids[1], ids[2] = ids[2], ids[1]
    triangles.append(ids)
bind = []
for b in mesh['bones']:
    q = b['rotation']
    m = Matrix.LocRotScale(Vector(b['translation']), Quaternion((q[3], *q[:3])), Vector(b['scale']))
    bind.append(bind[b['parent']]@m if b['parent'] >= 0 else m)
inverse = [m.inverted() for m in bind]

def skin(frame):
    snap = motion['frames'][frame]['pose']['Snapshot']
    entries = dict(zip(snap['BoneNames'], snap['LocalTransforms'], strict=True))
    pose = []
    for b in mesh['bones']:
        e = entries[b['name']]
        m = Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),
            Quaternion([e['Rotation'][k] for k in 'WXYZ']), Vector([e['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[b['parent']]@m if b['parent'] >= 0 else m)
    transforms = np.asarray([np.asarray(m@b) for m, b in zip(pose, inverse)])
    xyz = np.zeros_like(points)
    np.add.at(xyz, vi, (np.einsum('nij,nj->ni', transforms[bi, :3, :3], points[vi])+transforms[bi, :3, 3])*weights[:, None])
    return xyz

reports = []
for frame in (2, 3):
    before, after = skin(frame-1), skin(frame)
    contact = trace['cases'][frame]['largest_contact_corrections'][0]
    substep = int(contact['phase'].split('-')[1])
    alpha = (substep+1)/trace['substeps']
    xyz = before+(after-before)*alpha
    tree = BVHTree.FromPolygons(xyz.tolist(), triangles, all_triangles=True)
    samples = []
    for v in contact['body_vertices']:
        assert arm_mass[v] > .9
        point = Vector(xyz[v])
        near, normal, face, distance = tree.find_nearest(point)
        crossings = []
        for axis in range(3):
            for sign in (-1, 1):
                direction = Vector((0, 0, 0))
                direction[axis] = sign
                origin, count = point.copy(), 0
                for _ in range(64):
                    hit, _, _, _ = tree.ray_cast(origin, direction, 500)
                    if hit is None:
                        break
                    count += 1
                    origin = hit+direction*.0001
                crossings.append(count)
        samples.append({'body_vertex': v, 'position_cm': list(point),
            'signed_cm': (point-near).dot(normal), 'nearest_distance_cm': distance,
            'nearest_torso_vertices': triangles[face], 'six_ray_crossings': crossings})
    reports.append({'frame': frame, 'substep': substep, 'interpolation': alpha,
                    'cloth_contact': contact, 'samples': samples})
scope = 'Local torso-surface signed distances at the exact traced substeps. Torso subset is open at limb/neck boundaries, so ray parity alone is not a closed-volume containment proof. No body, pose or clothing edits.'
output.write_text(json.dumps({'scope': scope, 'torso_triangles': len(triangles), 'cases': reports}, indent=2)+'\n')
for r in reports:
    print(r['frame'], [(s['body_vertex'], round(s['signed_cm'], 4), s['six_ray_crossings']) for s in r['samples']])

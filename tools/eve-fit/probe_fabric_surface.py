"""Offline fabric reference with distance constraints, moving anchors and body contact."""
import argparse
import json
import math
import heapq
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2]/'work/eve26'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--clip', choices=('walk', 'jog', 'sprint'), default='sprint')
p.add_argument('--end', type=int, default=17)
p.add_argument('--name', required=True)
p.add_argument('--morph', choices=('default', 'hip-waist'), default='default')
p.add_argument('--substeps', type=int, default=4)
p.add_argument('--iterations', type=int, default=8)
p.add_argument('--surface-contact', action='store_true')
p.add_argument('--pin-z', type=float, default=112.)
p.add_argument('--free-z', type=float, default=103.)
p.add_argument('--contact-region', choices=('all', 'torso-legs'), default='all',
               help='Diagnostic collider isolation only; final clearance still measures the whole body')
p.add_argument('--velocity-contact', action='store_true', help='Limit post-projection separating velocity relative to moving body contacts')
p.add_argument('--mass', choices=('graded', 'uniform'), default='graded')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert a.name.isalnum()
assert 1 <= a.substeps <= 64 and 1 <= a.iterations <= 64
assert a.pin_z > a.free_z
output = work/f'{a.name}.json'
assert not output.exists()
mesh = json.loads((work/'holiday-hip-clean.mesh.json').read_text())
surface = json.loads((work/'skirt-f11.json').read_text())
source = json.loads((work/f'follow-{a.clip}-base.json').read_text())
ids = np.asarray(surface['source_vertices'], dtype=int)
points = np.asarray(mesh['points'], dtype=float)
if a.morph == 'hip-waist':
    for morph in mesh['morph_targets']:
        if morph['name'] in ('PBMHipSize', 'PBMWaistWidth'):
            for v, *delta in morph['deltas']:
                points[v] += delta
rest = points[ids]
faces = np.asarray(surface['indices'], dtype=int).reshape((-1, 3))
opposites = defaultdict(list)
for tri in faces:
    for j in range(3):
        opposites[tuple(sorted((int(tri[j]), int(tri[(j+1) % 3]))))].append(int(tri[(j+2) % 3]))
edges = np.asarray(sorted(opposites), dtype=int)
bends = np.asarray([v for v in opposites.values() if len(v) == 2], dtype=int)

def colors(pairs):
    used = [set() for _ in rest]
    groups = defaultdict(list)
    for i, (v, w) in enumerate(pairs):
        color = 0
        while color in used[v] or color in used[w]:
            color += 1
        used[v].add(color)
        used[w].add(color)
        groups[color].append(i)
    return [np.asarray(v, dtype=int) for v in groups.values()]

edge_lengths = np.linalg.norm(rest[edges[:, 0]]-rest[edges[:, 1]], axis=1)
bend_lengths = np.linalg.norm(rest[bends[:, 0]]-rest[bends[:, 1]], axis=1)
edge_colors, bend_colors = colors(edges), colors(bends)
inverse_mass = np.clip((a.pin_z-rest[:, 2])/(a.pin_z-a.free_z), 0., 1.)
inverse_mass = inverse_mass**2*(3.-2.*inverse_mass)
if a.mass == 'uniform':
    inverse_mass = (rest[:, 2] < a.pin_z).astype(float)
free = np.flatnonzero(inverse_mass > 0)
pinned = inverse_mass == 0
active_faces = np.flatnonzero(np.sum(inverse_mass[faces], axis=1) > 0)
contact_samples = [(tri, np.full(3, 1/3)) for tri in faces[active_faces]]
contact_samples += [(pair, np.full(2, .5)) for pair in edges
                    if np.sum(inverse_mass[pair]) > 0]
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    m = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@m if bone['parent'] >= 0 else m)
inverse = [m.inverted() for m in bind]
rows = np.asarray(mesh['influences'])
vi, bi, weights = rows[:, 0].astype(int), rows[:, 1].astype(int), rows[:, 2]
count = json.loads((work/'holiday.mesh.audit.json').read_text())['parts'][0]['faces']
body = []
for f in mesh['faces'][:count]:
    vertices = [mesh['wedges'][w][0] for w in f[:3]]
    normal = np.sum([mesh['normals'][w] for w in f[:3]], axis=0)
    if np.dot(np.cross(points[vertices[1]]-points[vertices[0]], points[vertices[2]]-points[vertices[0]]), normal) < 0:
        vertices[1], vertices[2] = vertices[2], vertices[1]
    body.append(vertices)
body_all = list(body)
if a.contact_region == 'torso-legs':
    arm_mass = np.zeros(len(points))
    for v, bone, weight in mesh['influences']:
        name = mesh['bones'][bone]['name']
        if any(part in name for part in ('upperarm', 'lowerarm', 'hand', 'thumb', 'index', 'middle', 'ring', 'pinky')):
            arm_mass[v] += weight
    body = [tri for tri in body_all if not all(arm_mass[v] > .5 for v in tri)]

def skin(frame):
    snapshot = frame['pose']['Snapshot']
    local = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
    pose = []
    for bone in mesh['bones']:
        e = local[bone['name']]
        m = Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),
            Quaternion([e['Rotation'][k] for k in 'WXYZ']), Vector([e['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[bone['parent']]@m if bone['parent'] >= 0 else m)
    mats = np.asarray([np.asarray(m@b) for m, b in zip(pose, inverse)])
    xyz = np.zeros_like(points)
    np.add.at(xyz, vi, (np.einsum('nij,nj->ni', mats[bi, :3, :3], points[vi])+mats[bi, :3, 3])*weights[:, None])
    return xyz

def distance_pass(x, pairs, lengths, groups, multipliers, alpha):
    for group in groups:
        v, w = pairs[group, 0], pairs[group, 1]
        delta = x[v]-x[w]
        length = np.linalg.norm(delta, axis=1)
        denominator = inverse_mass[v]+inverse_mass[w]+alpha
        valid = (denominator > 0) & (length > 1e-8)
        increment = np.zeros(len(group))
        increment[valid] = (-(length[valid]-lengths[group][valid])-alpha*multipliers[group][valid])/denominator[valid]
        multipliers[group] += increment
        correction = delta/np.maximum(length[:, None], 1e-8)*increment[:, None]
        x[v] += inverse_mass[v, None]*correction
        x[w] -= inverse_mass[w, None]*correction

def collision_pass(x, tree):
    for v in free:
        point = Vector(x[v])
        nearest, normal, body_face, _ = tree.find_nearest(point)
        signed = (point-nearest).dot(normal)
        if signed < .05:
            shift = np.asarray(normal)*(.05-signed)
            trace_contact('vertex', [v], shift[None, :], signed, body_face, normal)
            x[v] += shift
            if a.velocity_contact:
                velocity_contacts[((int(v),), (1.,))] = (np.asarray(normal), body_face)
    if a.surface_contact:
        for vertices, bary in contact_samples:
            mass = inverse_mass[vertices]
            denominator = float(np.sum(mass*bary*bary))
            if denominator < 1e-10:
                continue
            point = Vector(bary@x[vertices])
            nearest, normal, body_face, _ = tree.find_nearest(point)
            signed = (point-nearest).dot(normal)
            if signed < .05:
                correction = np.asarray(normal)*(.05-signed)/denominator
                shifts = (mass*bary)[:, None]*correction
                trace_contact('surface', vertices, shifts, signed, body_face, normal)
                x[vertices] += shifts
                if a.velocity_contact:
                    velocity_contacts[(tuple(int(v) for v in vertices), tuple(bary))] = (np.asarray(normal), body_face)

def trace_contact(kind, vertices, shifts, signed, body_face, normal):
    global contact_serial
    size = float(np.linalg.norm(shifts, axis=1).max())
    if len(contact_trace) == 8 and size <= contact_trace[0][0]:
        return
    row = {'kind': kind, 'shift_cm': size, 'signed_cm': signed, 'phase': phase,
        'source_vertices': [int(ids[v]) for v in vertices],
        'inverse_mass': [float(inverse_mass[v]) for v in vertices],
        'body_vertices': body[body_face], 'normal': list(normal)}
    contact_serial += 1
    entry = (size, contact_serial, row)
    if len(contact_trace) < 8:
        heapq.heappush(contact_trace, entry)
    else:
        heapq.heapreplace(contact_trace, entry)

frames, reports = [], []
previous = None
for fi, frame in enumerate(source['frames'][:a.end+1]):
    contact_trace, contact_serial = [], 0
    velocity_contacts = {}
    phase = 'settle'
    target = skin(frame)
    if previous is None:
        x = target[ids].copy()
        velocity = np.zeros_like(x)
        previous = target.copy()
        tree = BVHTree.FromPolygons(target.tolist(), body, all_triangles=True)
        edge_lambda, bend_lambda = np.zeros(len(edges)), np.zeros(len(bends))
        for _ in range(80):
            distance_pass(x, edges, edge_lengths, edge_colors, edge_lambda, 0.)
            distance_pass(x, bends, bend_lengths, bend_colors, bend_lambda, .0001*240.*240.)
            collision_pass(x, tree)
    dt = (frame['time']-source['frames'][fi-1]['time'])/a.substeps if fi else 1./(60*a.substeps)
    assert dt > 0
    for sub in range(a.substeps if fi else 0):
        phase = f'substep-{sub}'
        velocity_contacts = {}
        body_xyz = previous+(target-previous)*(sub+1)/a.substeps
        tree = BVHTree.FromPolygons(body_xyz.tolist(), body, all_triangles=True)
        old = x.copy()
        velocity *= math.exp(-4.*dt)
        velocity[free, 2] -= 980.*dt
        predicted_velocity = velocity.copy()
        x[free] += velocity[free]*dt
        x[pinned] = body_xyz[ids[pinned]]
        edge_lambda, bend_lambda = np.zeros(len(edges)), np.zeros(len(bends))
        for _ in range(a.iterations):
            distance_pass(x, edges, edge_lengths, edge_colors, edge_lambda, 0.)
            distance_pass(x, bends, bend_lengths, bend_colors, bend_lambda, .0001/(dt*dt))
            collision_pass(x, tree)
        velocity = (x-old)/dt
        if a.velocity_contact:
            body_velocity = (target-previous)/(dt*a.substeps)
            for _ in range(2):
                for (vertices, bary), (normal, body_face) in velocity_contacts.items():
                    vertices, bary = np.asarray(vertices), np.asarray(bary)
                    mass = inverse_mass[vertices]
                    denominator = float(np.sum(mass*bary*bary))
                    if denominator < 1e-10:
                        continue
                    collider_velocity = body_velocity[body[body_face]].mean(axis=0)
                    incoming = float(np.dot(bary@predicted_velocity[vertices]-collider_velocity, normal))
                    outgoing = float(np.dot(bary@velocity[vertices]-collider_velocity, normal))
                    impulse = (max(0., incoming)-outgoing)/denominator
                    velocity[vertices] += (mass*bary)[:, None]*normal*impulse
        velocity[pinned] = 0
    tree = BVHTree.FromPolygons(target.tolist(), body_all, all_triangles=True)
    signed = []
    for v in free:
        point = Vector(x[v])
        nearest, normal, _, _ = tree.find_nearest(point)
        signed.append((point-nearest).dot(normal))
    lengths = np.linalg.norm(x[edges[:, 0]]-x[edges[:, 1]], axis=1)
    active = ((inverse_mass[edges[:, 0]]+inverse_mass[edges[:, 1]]) > 0) & (edge_lengths > .01)
    ratios = lengths[active]/edge_lengths[active]
    sample_hits = []
    for face_id in active_faces:
        tri = faces[face_id]
        for bary in ((1/3, 1/3, 1/3), (.5, .5, 0), (0, .5, .5), (.5, 0, .5)):
            position = Vector(np.asarray(bary)@x[tri])
            nearest, normal, body_face, _ = tree.find_nearest(position)
            distance = (position-nearest).dot(normal)
            if distance < -.1:
                sample_hits.append({'face': int(face_id), 'barycentric': bary,
                    'movable': bool(np.sum(inverse_mass[tri]*np.asarray(bary)**2) > 1e-10),
                    'signed_cm': distance, 'body_triangle': body_face})
    sample_hits.sort(key=lambda row: row['signed_cm'])
    reports.append({'frame': fi, 'time': frame['time'], 'min_signed_cm': min(signed),
        'over_1mm': sum(v < -.1 for v in signed), 'edge_ratio_max': float(ratios.max()),
        'edge_ratio_p95': float(np.percentile(ratios, 95)), 'edge_ratio_min': float(ratios.min()),
        'max_from_skin_cm': float(np.linalg.norm(x-target[ids], axis=1).max()),
        'face_samples_over_1mm': len(sample_hits),
        'worst_face_samples': sample_hits[:3],
        'largest_contact_corrections': [entry[2] for entry in sorted(contact_trace, reverse=True)],
        'max_speed_cm_s': float(np.linalg.norm(velocity[free], axis=1).max())})
    assert np.isfinite(x).all() and np.max(np.abs(x)) < 10000, reports[-1]
    frames.append({'frame': fi, 'positions_cm': x.tolist()})
    previous = target
    if fi % 4 == 0:
        print(reports[-1], flush=True)
scope = 'Offline fabric reference: hard edge distances, compliant opposite-vertex bending, pinned upper dress and nearest-triangle contact. Optional barycentric surface-contact projection. Initial settling has zero velocity. No self collision, CCD, friction, trim attachments or runtime integration. Not accepted fitting.'
output.write_text(json.dumps({'scope': scope, 'source_motion': str(work/f'follow-{a.clip}-base.json'),
    'substeps': a.substeps, 'iterations': a.iterations, 'surface_contact': a.surface_contact,
    'pin_z_cm': a.pin_z, 'free_z_cm': a.free_z,
    'contact_region': a.contact_region, 'clearance_region': 'whole body',
    'velocity_contact': a.velocity_contact,
    'mass': a.mass,
    'morph_case': a.morph, 'source_vertices': ids.tolist(), 'free_vertices': len(free),
    'frames': frames, 'cases': reports}, separators=(',', ':')))
print('Finished', len(frames), 'frames', flush=True)

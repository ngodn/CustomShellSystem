"""Measure continuity and shape cost of persistent hem contact projection offline."""
import argparse
import copy
import json
import math
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2] / 'work/eve26'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--clip', choices=('walk', 'jog', 'sprint'), default='sprint')
parser.add_argument('--name', required=True)
parser.add_argument('--morph', choices=('default', 'hip-waist'), default='default')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
assert args.name.isalnum()
output = work / f'{args.name}-pose.json'
receipt = work / f'{args.name}.json'
assert not output.exists() and not receipt.exists()
mesh = json.loads((work/'holiday-hem2.mesh.json').read_text())
source = json.loads((work/f'follow-{args.clip}-base.json').read_text())
mapping = json.loads((work/'skirt-hem2.json').read_text())['drivers']
indices = {b['name']: i for i, b in enumerate(mesh['bones'])}
carriers = [indices[n] for n in mapping]
points = np.asarray(mesh['points'], dtype=float)
if args.morph == 'hip-waist':
    for morph in mesh['morph_targets']:
        if morph['name'] in ('PBMHipSize', 'PBMWaistWidth'):
            for v, *delta in morph['deltas']:
                points[v] += delta
bind = []
for b in mesh['bones']:
    q = b['rotation']
    m = Matrix.LocRotScale(Vector(b['translation']), Quaternion((q[3], *q[:3])), Vector(b['scale']))
    bind.append(bind[b['parent']] @ m if b['parent'] >= 0 else m)
inverse = [b.inverted() for b in bind]
rows = np.asarray(mesh['influences'])
vi, bi, weights = rows[:, 0].astype(int), rows[:, 1].astype(int), rows[:, 2]
W = np.zeros((len(points), len(carriers)))
for j, bone in enumerate(carriers):
    mask = bi == bone
    np.add.at(W[:, j], vi[mask], weights[mask])
body_count = json.loads((work/'holiday.mesh.audit.json').read_text())['parts'][0]['faces']
triangles = []
for face in mesh['faces'][:body_count]:
    ids = [mesh['wedges'][w][0] for w in face[:3]]
    normal = np.sum([mesh['normals'][w] for w in face[:3]], axis=0)
    if np.dot(np.cross(points[ids[1]]-points[ids[0]], points[ids[2]]-points[ids[0]]), normal) < 0:
        ids[1], ids[2] = ids[2], ids[1]
    triangles.append(ids)
slot = mesh['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
vertices, edges = set(), set()
for face in mesh['faces']:
    if face[3] != slot:
        continue
    ids = [mesh['wedges'][w][0] for w in face[:3]]
    vertices.update(v for v in ids if mesh['points'][v][2] < 120)
    for j in range(3):
        a, b = ids[j], ids[(j+1) % 3]
        if W[a].sum() or W[b].sum():
            edges.add(tuple(sorted((a, b))))
vertices = sorted(vertices)
edges = np.asarray(sorted(edges))
shifts = np.zeros((len(carriers), 3))
previous_delta = np.zeros_like(points)
frames, reports = [], []
previous_basis = None
for fi, frame in enumerate(source['frames']):
    snapshot = frame['pose']['Snapshot']
    local = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
    pose = []
    for b in mesh['bones']:
        e = local[b['name']]
        m = Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),
            Quaternion([e['Rotation'][k] for k in 'WXYZ']), Vector([e['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[b['parent']] @ m if b['parent'] >= 0 else m)
    for target, driver in mapping.items():
        t, d = indices[target], indices[driver]
        pose[t] = pose[d] @ inverse[d] @ bind[t]
    mats = np.asarray([np.asarray(p @ inv) for p, inv in zip(pose, inverse)])
    xyz = np.zeros_like(points)
    np.add.at(xyz, vi, (np.einsum('nij,nj->ni', mats[bi, :3, :3], points[vi]) + mats[bi, :3, 3])*weights[:, None])
    tree = BVHTree.FromPolygons(xyz.tolist(), triangles, all_triangles=True)
    basis = np.asarray(pose[indices['pelvis']].to_quaternion().to_matrix())
    dt = frame['time']-source['frames'][fi-1]['time'] if fi else 0.
    if previous_basis is not None:
        shifts = (basis @ previous_basis.T @ shifts.T).T * math.exp(-dt / .08)
    previous_basis = basis

    def contacts():
        hits = []
        for v in vertices:
            p = Vector(xyz[v]+W[v]@shifts)
            near, normal, _, _ = tree.find_nearest(p)
            signed = (p-near).dot(normal)
            if signed < .025:
                hits.append((signed, v))
        return sorted(hits)

    for _ in range(12):
        for _, v in contacts():
            p = Vector(xyz[v]+W[v]@shifts)
            near, normal, _, _ = tree.find_nearest(p)
            signed = (p-near).dot(normal)
            gradient = W[v, :, None]*np.asarray(normal)[None, :]
            denominator = float(np.sum(gradient**2))
            if signed >= .025 or denominator < .001:
                continue
            shifts += gradient*(.025-signed)/denominator*.35
            shifts *= np.minimum(1., 8./np.maximum(np.linalg.norm(shifts, axis=1), 1e-9))[:, None]
    hits = contacts()
    delta = W@shifts
    corrected = xyz+delta
    before = np.linalg.norm(xyz[edges[:, 0]]-xyz[edges[:, 1]], axis=1)
    after = np.linalg.norm(corrected[edges[:, 0]]-corrected[edges[:, 1]], axis=1)
    valid = before > .01
    ratios = after[valid]/before[valid]
    reports.append({'frame': fi, 'time': frame['time'],
        'min_signed_cm': hits[0][0] if hits else .025,
        'over_1mm': sum(s < -.1 for s, _ in hits),
        'controlled_over_1mm': int(sum(s < -.1 and W[v].sum() > .1 for s, v in hits)),
        'max_shift_cm': float(np.linalg.norm(delta, axis=1).max()),
        'max_correction_step_cm': float(np.linalg.norm(delta-previous_delta, axis=1).max()) if fi else 0.,
        'edge_ratio_min': float(ratios.min()), 'edge_ratio_max': float(ratios.max()),
        'edge_ratio_p95': float(np.percentile(ratios, 95))})
    previous_delta = delta
    for j, bone in enumerate(carriers):
        pose[bone] = pose[bone].copy()
        pose[bone].translation += Vector(shifts[j])
    result = copy.deepcopy(frame)
    entries = dict(zip(result['pose']['Snapshot']['BoneNames'], result['pose']['Snapshot']['LocalTransforms'], strict=True))
    for i, b in enumerate(mesh['bones']):
        m = pose[b['parent']].inverted()@pose[i] if b['parent'] >= 0 else pose[i]
        t, q, s = m.decompose()
        entries[b['name']].update(Translation=dict(zip('XYZ', t)),
            Rotation=dict(zip('XYZW', (q.x, q.y, q.z, q.w))), Scale3D=dict(zip('XYZ', s)))
    frames.append(result)
    if fi % 8 == 0:
        print(reports[-1], flush=True)
scope = 'Offline persistent contact projection with 80 ms decay, 8 cm carrier cap, no inertial cloth dynamics or edge constraints. Diagnostic only, not accepted fitting or runtime code.'
output.write_text(json.dumps({'scope': scope, 'morph_case': args.morph, 'frames': frames}, separators=(',', ':')))
receipt.write_text(json.dumps({'scope': scope, 'clip': args.clip, 'morph_case': args.morph,
    'cases': reports}, indent=2)+'\n')
print('Finished', len(frames), 'frames', flush=True)

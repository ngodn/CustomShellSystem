"""Measure rigid regional hull coverage before creating a native collision asset."""
import argparse
import json
import sys
from pathlib import Path

import bmesh
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.output.mkdir(exist_ok=False)
w = Path(__file__).resolve().parents[2]/'work/eve26'
mesh = json.loads((w/'holiday-waist-source.mesh.json').read_text())
body = json.loads((w/'body-collider.json').read_text())
points = np.asarray(mesh['points'])
body_points = np.asarray(body['positions'])
assert np.array_equal(points[:len(body_points)], body_points)
names = {bone['name']: i for i, bone in enumerate(mesh['bones'])}
regions = ('pelvis', 'spine_01', 'spine_02', 'butt001', 'butt002', 'thigh_l', 'thigh_r')
mass = np.zeros((len(body_points), len(regions)))
arm_mass = np.zeros(len(body_points))
for i, weights in enumerate(body['weights']):
    for name, weight in weights:
        if '_twist_' in name:
            name = name.split('_twist_')[0]+'_'+name[-1]
        if name in regions:
            mass[i, regions.index(name)] += weight
        if any(s in name for s in ('arm', 'hand', 'thumb', 'index', 'middle', 'ring', 'pinky')):
            arm_mass[i] += weight
roi = np.flatnonzero((body_points[:, 2] >= 90)&(body_points[:, 2] <= 125)&(arm_mass < .25))
face_ids = [f for f in body['indices'] if not any(arm_mass[v] > .25 for v in f)]
body_faces = [[c, b, a] for a, b, c in face_ids]
hulls = []
for j, name in enumerate(regions):
    ids = np.flatnonzero((mass[:, j] >= .25)&(body_points[:, 2] >= 75)&(body_points[:, 2] <= 142)&(arm_mass < .25))
    bm = bmesh.new()
    for point in body_points[ids]:
        bm.verts.new(point)
    bmesh.ops.convex_hull(bm, input=list(bm.verts), use_existing_faces=False)
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    used = {vertex for face in bm.faces for vertex in face.verts}
    vertices = list(used)
    lookup = {vertex: i for i, vertex in enumerate(vertices)}
    rest = np.asarray([list(vertex.co) for vertex in vertices])
    center = rest.mean(axis=0)
    faces = []
    for face in bm.faces:
        ids3 = [lookup[vertex] for vertex in face.verts]
        tri = rest[ids3]
        if np.dot(np.cross(tri[1]-tri[0], tri[2]-tri[0]), tri.mean(axis=0)-center) < 0:
            ids3.reverse()
        faces.append(ids3)
    bm.free()
    hulls.append({'bone': name, 'positions': rest.tolist(), 'indices': faces, 'source_vertices': len(ids)})
(a.output/'hulls.json').write_text(json.dumps({'scope': 'Offline overlapping regional convex hulls. Source mass >=0.25, rest Z75..142 cm, arms excluded. Not imported or accepted.', 'regions': hulls}, indent=2)+'\n')
print('Hull points', [(h['bone'], len(h['positions'])) for h in hulls], flush=True)
slot = mesh['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
garment = sorted({mesh['wedges'][wedge][0] for face in mesh['faces'] if face[3] == slot
                  for wedge in face[:3] if 90 <= points[mesh['wedges'][wedge][0], 2] < 120})
rows = np.asarray(mesh['influences'])
vi, bi, weight = rows[:, 0].astype(int), rows[:, 1].astype(int), rows[:, 2]
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@local if bone['parent'] >= 0 else local)
cases = {'default': points, 'hip-waist': points.copy()}
for morph in mesh['morph_targets']:
    if morph['name'] in ('PBMHipSize', 'PBMWaistWidth'):
        for i, *delta in morph['deltas']:
            cases['hip-waist'][i] += delta

def signed(tree, point):
    closest, normal, _, _ = tree.find_nearest(Vector(point))
    return (Vector(point)-closest).dot(normal)

def stats(values):
    v = np.asarray(values)
    return {'samples': len(v), 'minimum_cm': float(v.min()), 'maximum_cm': float(v.max()),
            'p05_cm': float(np.percentile(v, 5)), 'p95_cm': float(np.percentile(v, 95)),
            'outside_over_3mm': int((v > .3).sum()), 'inside_over_3mm': int((v < -.3).sum())}

reports = []
for clip in ('walk', 'jog', 'sprint'):
    frames = json.loads((w/f'follow-{clip}-base.json').read_text())['frames']
    selected = sorted(set(range(0, len(frames), 12)) | {len(frames)-1, 6 if clip == 'jog' else 20 if clip == 'sprint' else 4})
    for fi in selected:
        snapshot = frames[fi]['pose']['Snapshot']
        local = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
        pose = []
        for bone in mesh['bones']:
            t = local[bone['name']]
            m = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']), Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
            pose.append(pose[bone['parent']]@m if bone['parent'] >= 0 else m)
        transforms = np.asarray([np.asarray(m@b.inverted()) for m, b in zip(pose, bind, strict=True)])
        trees, hull_samples, posed_hulls = [], [], []
        for hull in hulls:
            matrix = transforms[names[hull['bone']]]
            xyz = np.asarray(hull['positions'])@matrix[:3, :3].T+matrix[:3, 3]
            trees.append(BVHTree.FromPolygons(xyz.tolist(), hull['indices'], all_triangles=True))
            hull_samples.extend(xyz.tolist())
            hull_samples.extend(xyz[np.asarray(hull['indices'])].mean(axis=1).tolist())
            posed_hulls.append({**hull, 'posed_cm': xyz.tolist()})
        for label, rest in cases.items():
            posed = np.zeros_like(rest)
            np.add.at(posed, vi, (np.einsum('nij,nj->ni', transforms[bi, :3, :3], rest[vi])+transforms[bi, :3, 3])*weight[:, None])
            body_tree = BVHTree.FromPolygons(posed[:len(body_points)].tolist(), body_faces, all_triangles=True)
            coverage = [min(signed(tree, point) for tree in trees) for point in posed[roi]]
            garment_hull = [min(signed(tree, point) for tree in trees) for point in posed[garment]]
            garment_body = [signed(body_tree, point) for point in posed[garment]]
            overfill = [signed(body_tree, point) for point in hull_samples]
            reports.append({'clip': clip, 'frame': fi, 'case': label, 'body_to_hull_union': stats(coverage),
                            'garment_to_hull_union': stats(garment_hull), 'garment_to_body': stats(garment_body),
                            'all_hull_samples_to_body': stats(overfill),
                            'worst_uncovered_body': int(roi[int(np.argmax(coverage))]),
                            'garment_clear_of_body_but_inside_hull': sum(b >= -.1 and h < -.3 for b, h in zip(garment_body, garment_hull))})
            if clip == 'sprint' and fi == 48 and label == 'default':
                (a.output/'sprint48.json').write_text(json.dumps({'hulls': posed_hulls, 'body_positions': posed[:len(body_points)].tolist(), 'body_faces': body_faces}, separators=(',', ':'))+'\n')
    print(clip, 'cases', len(reports), flush=True)
(a.output/'coverage.json').write_text(json.dumps({'scope': 'Sampled regional hull feasibility, not native collision or cloth acceptance. Min per-hull signed distances classify union membership but negative magnitudes are not exact union-boundary distances. Hull overfill samples include internal overlap faces. Body and garment morphs vary while hulls remain default-rest geometry. Arms excluded to isolate torso/legs.', 'cases': reports}, indent=2)+'\n')
print('Worst uncovered cm', max(r['body_to_hull_union']['maximum_cm'] for r in reports), flush=True)

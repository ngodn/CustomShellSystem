"""Compare posed body, collider, simulation surface and mapped garment in one frame."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--motion', type=Path, required=True)
p.add_argument('--mapped', type=Path, required=True)
p.add_argument('--proxy', type=Path, required=True)
p.add_argument('--mapping', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--zmax', type=float, default=140.)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
motion = json.loads(a.motion.read_text())
mapped = json.loads(a.mapped.read_text())
assert mapped['simulation_sha256'] == hashlib.sha256(a.motion.read_bytes()).hexdigest()
assert mapped['mapping_sha256'] == hashlib.sha256(a.mapping.read_bytes()).hexdigest()
mapping = json.loads(a.mapping.read_text())
frame = mapped['frame']
native = motion['frames'][frame]
mesh = json.loads((w/'holiday.mesh.json').read_text())
body = json.loads((w/'body-collider.json').read_text())
collider = json.loads(Path(motion['body_collision_input']).read_text())
proxy = json.loads(a.proxy.read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
source = json.loads(Path(motion['source_motion']).read_text())
snapshot = source['frames'][frame]['pose']['Snapshot']
entries = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
bind, pose = [], []
for bone in mesh['bones']:
    q = bone['rotation']
    b = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    t = entries[bone['name']]
    m = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']), Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
    parent = bone['parent']
    bind.append(bind[parent]@b if parent >= 0 else b)
    pose.append(pose[parent]@m if parent >= 0 else m)
transforms = np.asarray([np.asarray(m@b.inverted()) for m, b in zip(pose, bind)])
names = {b['name']: i for i, b in enumerate(mesh['bones'])}

def skin(points, weights):
    result = np.zeros_like(points)
    rows = np.asarray([(v, names[n], wt) for v, row in enumerate(weights) for n, wt in row])
    vi, bi, wt = rows[:, 0].astype(int), rows[:, 1].astype(int), rows[:, 2]
    np.add.at(result, vi, (np.einsum('nij,nj->ni', transforms[bi, :3, :3], points[vi])+transforms[bi, :3, 3])*wt[:, None])
    return result

def tree(points, faces):
    return BVHTree.FromPolygons(points, [[c, b, a] for a, b, c in faces], all_triangles=True)

body_tree = tree(skin(np.asarray(body['positions']), body['weights']).tolist(), body['indices'])
collider_tree = tree(native['body_reference_skin_cm'], collider['indices'])

def measure(points, surface):
    values = []
    for point in points:
        near, normal, _, distance = surface.find_nearest(Vector(point))
        values.append((Vector(point)-near).dot(normal))
    values = np.asarray(values)
    return {'samples': len(values), 'inside': int((values < 0).sum()),
            'inside_over_1mm': int((values < -.1).sum()),
            'min_signed_cm': float(values.min()), 'p05_signed_cm': float(np.percentile(values, 5)),
            'median_signed_cm': float(np.median(values)), 'p95_signed_cm': float(np.percentile(values, 95)),
            'worst': [{'sample': int(i), 'signed_cm': float(values[i])} for i in np.argsort(values)[:8]]}

rows = []
def check(label, rest, actual, faces):
    rest, actual, faces = np.asarray(rest), np.asarray(actual), np.asarray(faces).reshape((-1, 3))
    # Restrict both vertices and triangle centroids by their unposed height.
    bary = []
    for i in range(4):
        for j in range(4-i):
            bary.append([(i+1/3)/4, (j+1/3)/4, 1-(i+j+2/3)/4])
            if i+j < 3:
                bary.append([(i+2/3)/4, (j+2/3)/4, 1-(i+j+4/3)/4])
    assert len(bary) == 16
    dense_rest = np.einsum('si,fij->fsj', bary, rest[faces]).reshape((-1, 3))
    dense_actual = np.einsum('si,fij->fsj', bary, actual[faces]).reshape((-1, 3))
    for sample, r, pts in [('vertices', rest, actual), ('centroids', rest[faces].mean(axis=1), actual[faces].mean(axis=1)), ('subtriangle_centres16', dense_rest, dense_actual)]:
        roi = (r[:, 2] >= 90) & (r[:, 2] <= a.zmax)
        pts = pts[roi]
        row = {'surface': label, 'sampling': sample, 'body': measure(pts, body_tree), 'collider': measure(pts, collider_tree)}
        for surface in ('body', 'collider'):
            for item in row[surface]['worst']:
                i = item['sample']
                item['rest_cm'] = r[roi][i].tolist()
                item['posed_cm'] = pts[i].tolist()
                item['source_sample'] = int(np.flatnonzero(roi)[i])
        rows.append(row)
        print({**row, **{s: {k: v for k, v in row[s].items() if k != 'worst'} for s in ('body', 'collider')}}, flush=True)

check('simulation', proxy['positions'], native['positions_cm'], proxy['indices'])
section = mapping['render_geometry']['sections'][1]
actual = mapped['sections'][1]
assert 'XM_Dress01' in mapping['sections'][1]['material']
check('mapped_fabric', section['positions'], actual['positions_cm'], section['indices'])
check('skin_only_fabric', section['positions'], skin(np.asarray(section['positions']), section['weights']), section['indices'])
lookup = KDTree(len(mesh['points']))
for i, point in enumerate(mesh['points']):
    lookup.insert(Vector(point), i)
lookup.balance()
latest = json.loads((w/'holiday-hip-clean.mesh.json').read_text())
records = np.asarray(section['mapping']).reshape((len(section['positions']), -1, 9))
clipped = []
for i, (rest, point) in enumerate(zip(section['positions'], actual['positions_cm'], strict=True)):
    if not 90 <= rest[2] <= a.zmax:
        continue
    near, normal, face, _ = body_tree.find_nearest(Vector(point))
    signed = (Vector(point)-near).dot(normal)
    if signed >= -.1:
        continue
    _, source_vertex, source_error = lookup.find(Vector(rest))
    body_weights = {}
    for v in body['indices'][face]:
        for bone, weight in body['weights'][v]:
            body_weights[bone] = body_weights.get(bone, 0.) + weight/3
    clipped.append({'render_vertex': i, 'rest_cm': rest, 'signed_cm': signed,
        'fully_skinned': bool(np.all(records[i, :, 3] == 65535)),
        'source_vertex': source_vertex, 'source_match_cm': source_error,
        'latest_source_offset_cm': float(np.linalg.norm(np.asarray(latest['points'][source_vertex])-mesh['points'][source_vertex])),
        'garment_weights': section['weights'][i],
        'nearest_body_face_weights': sorted(body_weights.items(), key=lambda x: -x[1])})
a.output.write_text(json.dumps({'scope': 'One default-morph frame, vertices, triangle centroids and 16 subtriangle centres. Signed nearest-normal distances may be ambiguous at folds. Collider is native shape API reference, not solver internal readback. Not full-surface or game acceptance.', 'rest_z_range_cm': [90, a.zmax],
    'frame': frame, 'motion': str(a.motion), 'mapped': str(a.mapped), 'rows': rows,
    'clipped_mapped_vertices': clipped}, indent=2)+'\n')

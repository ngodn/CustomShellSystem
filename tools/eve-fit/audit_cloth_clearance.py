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
p.add_argument('--source-mesh', type=Path, help='Exact source export for native vertex tracing')
p.add_argument('--support-detail', action='store_true', help='Trace cloth supports and movement limits for clipped dynamic vertices')
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
source_mesh_path = a.source_mesh or w/'holiday.mesh.json'
mesh = json.loads(source_mesh_path.read_text())
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
skin_positions = skin(np.asarray(section['positions']), section['weights'])
check('skin_only_fabric', section['positions'], skin_positions, section['indices'])
lookup = KDTree(len(mesh['points']))
for i, point in enumerate(mesh['points']):
    lookup.insert(Vector(point), i)
lookup.balance()
latest = mesh if a.source_mesh else json.loads((w/'holiday-hip-clean.mesh.json').read_text())
records = np.asarray(section['mapping']).reshape((len(section['positions']), -1, 9))
support_ids = set()
simulated = np.asarray(native['positions_cm'])
clipped = []
for i, (rest, point) in enumerate(zip(section['positions'], actual['positions_cm'], strict=True)):
    if not 90 <= rest[2] <= a.zmax:
        continue
    near, normal, face, _ = body_tree.find_nearest(Vector(point))
    signed = (Vector(point)-near).dot(normal)
    if signed >= -.1:
        continue
    _, source_vertex, source_error = lookup.find(Vector(rest))
    skin_near, skin_normal, _, _ = body_tree.find_nearest(Vector(skin_positions[i]))
    collider_near, collider_normal, _, _ = collider_tree.find_nearest(Vector(point))
    flags = records[i, :, 3]
    blend = float(np.where(flags < 65535, 1.-flags/65535., 0.).mean())
    body_weights = {}
    for v in body['indices'][face]:
        for bone, weight in body['weights'][v]:
            body_weights[bone] = body_weights.get(bone, 0.) + weight/3
    record = {'render_vertex': i, 'rest_cm': rest, 'signed_cm': signed,
        'cloth_blend': blend,
        'skin_only_signed_cm': (Vector(skin_positions[i])-skin_near).dot(skin_normal),
        'collider_signed_cm': (Vector(point)-collider_near).dot(collider_normal),
        'cloth_displacement_cm': float(np.linalg.norm(np.asarray(point)-skin_positions[i])),
        'fully_skinned': bool(np.all(records[i, :, 3] == 65535)),
        'source_vertex': source_vertex, 'source_match_cm': source_error,
        'latest_source_offset_cm': float(np.linalg.norm(np.asarray(latest['points'][source_vertex])-mesh['points'][source_vertex])),
        'garment_weights': section['weights'][i],
        'nearest_body_face_weights': sorted(body_weights.items(), key=lambda x: -x[1])}
    if a.support_detail and blend > 0:
        active = [row for row in records[i] if row[3] < 65535 and row[8] > 0]
        total = sum(row[8] for row in active)
        assert total > 0
        zero_offset = np.zeros(3)
        influences = []
        for row in active:
            ids = row[:3].astype(int)
            bary = np.asarray([row[4], row[5], 1-row[4]-row[5]])
            zero_offset += (bary[:, None]*simulated[ids]).sum(axis=0)*row[8]/total
            support_ids.update(int(v) for v in ids)
            influences.append({'vertices': ids.tolist(), 'barycentric': bary.tolist(),
                               'normal_offset_cm': float(row[7]), 'weight': float(row[8]/total)})
        zero_offset = zero_offset*blend + skin_positions[i]*(1-blend)
        near_zero, normal_zero, _, _ = body_tree.find_nearest(Vector(zero_offset))
        record['without_normal_offset_signed_cm'] = (Vector(zero_offset)-near_zero).dot(normal_zero)
        record['supports'] = influences
    clipped.append(record)
support_rows = []
if a.support_detail:
    rest_proxy = np.asarray(proxy['positions'])
    skin_proxy = skin(rest_proxy, proxy['weights'])
    alpha = np.clip((proxy['anchor_top_cm']-20-rest_proxy[:, 2])/12, 0, 1)
    limits = 18*alpha**2*(3-2*alpha)
    for vertex in sorted(support_ids):
        record = {'vertex': vertex, 'rest_cm': rest_proxy[vertex].tolist(),
                  'max_distance_cm': float(limits[vertex]),
                  'displacement_cm': float(np.linalg.norm(simulated[vertex]-skin_proxy[vertex]))}
        for name, positions in (('simulated', simulated), ('skinned', skin_proxy)):
            for surface, bvh in (('body', body_tree), ('collider', collider_tree)):
                near, normal, _, _ = bvh.find_nearest(Vector(positions[vertex]))
                record[name+'_'+surface+'_signed_cm'] = (Vector(positions[vertex])-near).dot(normal)
        support_rows.append(record)
a.output.write_text(json.dumps({'scope': 'One default-morph frame, vertices, triangle centroids and 16 subtriangle centres. Signed nearest-normal distances may be ambiguous at folds. Collider is native shape API reference, not solver internal readback. Not full-surface or game acceptance.', 'rest_z_range_cm': [90, a.zmax],
    'frame': frame, 'motion': str(a.motion), 'mapped': str(a.mapped),
    'source_mesh': str(source_mesh_path), 'source_mesh_sha256': hashlib.sha256(source_mesh_path.read_bytes()).hexdigest(), 'rows': rows,
    'clipped_mapped_vertices': clipped, 'dynamic_support_particles': support_rows}, indent=2)+'\n')

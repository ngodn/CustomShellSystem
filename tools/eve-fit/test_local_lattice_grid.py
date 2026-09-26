"""Evaluate fixed-grid local weight transfer across the recorded locomotion poses."""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--subdivide', type=int, choices=(1, 2, 4), default=1)
p.add_argument('--surface-frame', help='Export an approximate mapped body surface at clip:frame.')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.output.mkdir(exist_ok=False)
w = Path(__file__).resolve().parents[2]/'work/eve26'
native = json.loads(a.input.read_text())
assert native['fresh_load'] and len(native['bodies']) == 1
collider = native['bodies'][0]
grid = collider['lattice_geometry']
nodes = grid['nodes']
counts = np.asarray(grid['counts'], dtype=int)
assert len(nodes) == int(np.prod(counts+1))
lookup = {tuple(n['index']): i for i, n in enumerate(nodes)}
assert len(lookup) == len(nodes)
node_rest = np.asarray([n['rest_cm'] for n in nodes])
origin = node_rest[lookup[(0, 0, 0)]]
axes = np.column_stack([node_rest[lookup[tuple(int(i == j) for i in range(3))]]-origin for j in range(3)])
target_counts = counts*a.subdivide
target_nodes = nodes if a.subdivide == 1 else [
    {'index':[x,y,z], 'rest_cm':(origin+axes@np.asarray([x,y,z])/a.subdivide).tolist()}
    for x in range(target_counts[0]+1) for y in range(target_counts[1]+1) for z in range(target_counts[2]+1)]
target_rest = np.asarray([n['rest_cm'] for n in target_nodes])
target_lookup = {tuple(n['index']): i for i,n in enumerate(target_nodes)}
mesh = json.loads((w/'cbody.mesh.json').read_text())
body = json.loads((w/'body-collider.json').read_text())
points = np.asarray(body['positions'])
assert np.array_equal(points, np.asarray(mesh['points']))
names = {b['name']: i for i, b in enumerate(mesh['bones'])}
root_index = names[collider['root_bone']]
tree = BVHTree.FromPolygons(points.tolist(), body['indices'], all_triangles=True)
revised_nodes = []
max_dropped = 0.
clamped_count, max_closest_error = 0, 0.
for node in target_nodes:
    closest, normal, face_id, distance = tree.find_nearest(Vector(node['rest_cm']))
    assert face_id is not None
    face = body['indices'][face_id]
    triangle = points[face]
    uv = np.linalg.lstsq(np.column_stack((triangle[1]-triangle[0], triangle[2]-triangle[0])), np.asarray(closest)-triangle[0], rcond=None)[0]
    bary = np.asarray([1-uv.sum(), *uv])
    clamped_count += int(bary.min() < 0 or bary.max() > 1)
    bary = np.clip(bary, 0, 1); bary /= bary.sum()
    closest_error = float(np.linalg.norm(bary@triangle-np.asarray(closest)))
    assert closest_error < .0001, {'node':node['index'],'face':face_id,'error_cm':closest_error}
    max_closest_error = max(max_closest_error, closest_error)
    weights = {}
    for vertex, amount in zip(face, bary, strict=True):
        for name, weight in body['weights'][vertex]:
            weights[name] = weights.get(name, 0)+float(amount)*weight
    ordered = sorted(((name, weight) for name, weight in weights.items() if weight > 1e-8), key=lambda row: -row[1])
    max_dropped = max(max_dropped, sum(weight for _, weight in ordered[12:]))
    total = sum(weight for _, weight in ordered[:12])
    revised_nodes.append({**node, 'weights': [[name, weight/total] for name, weight in ordered[:12]],
                          'nearest_body_face': face_id, 'barycentric': bary.tolist(), 'surface_distance_cm': distance})
recipe = {'source_asset': native['asset'], 'root_bone': collider['root_bone'],
          'source_grid_sha256': hashlib.sha256(a.input.read_bytes()).hexdigest(),
          'body_sha256': hashlib.sha256((w/'body-collider.json').read_bytes()).hexdigest(),
          'grid': {'counts': target_counts.tolist(), 'nodes': revised_nodes}, 'subdivision':a.subdivide,
          'scope': 'Offline local weight recipe with fixed domain bounds, not a saved collider or accepted fit.'}
(a.output/'weights.json').write_text(json.dumps(recipe, separators=(',', ':'))+'\n')

# Reconstruct the engine's five-tetrahedron cell subdivision in component space.
even = np.asarray([[(0,1,0),(0,0,0),(1,1,0),(0,1,1)],[(1,1,1),(1,1,0),(1,0,1),(0,1,1)],
                   [(1,0,0),(1,0,1),(1,1,0),(0,0,0)],[(0,0,1),(0,0,0),(0,1,1),(1,0,1)],[(1,1,0),(0,1,1),(0,0,0),(1,0,1)]])
odd = np.asarray([[(0,0,0),(1,0,0),(0,1,0),(0,0,1)],[(0,1,1),(0,1,0),(1,1,1),(0,0,1)],
                  [(1,0,1),(1,1,1),(1,0,0),(0,0,1)],[(1,1,0),(0,1,0),(1,0,0),(1,1,1)],[(1,0,0),(1,1,1),(0,1,0),(0,0,1)]])
coordinates = (points-origin)@np.linalg.inv(axes).T
def embed(coordinates, counts, lookup):
    cells = np.floor(coordinates).astype(int)
    assert (cells >= 0).all() and (cells < counts).all()
    alpha = coordinates-cells
    indices = np.full((len(points), 4), -1, dtype=int)
    coefficients = np.zeros((len(points), 4))
    for parity, tetrahedra in enumerate((even, odd)):
        for offsets in tetrahedra:
            selected = np.flatnonzero(((cells.sum(axis=1) % 2) == parity)&(indices[:, 0] < 0))
            basis = np.vstack((offsets.T, np.ones(4)))
            bary = np.column_stack((alpha[selected], np.ones(len(selected))))@np.linalg.inv(basis).T
            valid = (bary >= -1e-9).all(axis=1)
            selected, bary = selected[valid], bary[valid]
            if not len(selected):
                continue
            indices[selected] = [[lookup[tuple(cell+offset)] for offset in offsets] for cell in cells[selected]]
            coefficients[selected] = bary
    assert (indices >= 0).all()
    return cells, indices, coefficients
cells, indices, coefficients = embed(coordinates, counts, lookup)
_, target_indices, target_coefficients = embed(coordinates*a.subdivide, target_counts, target_lookup)
rest_error = np.linalg.norm(np.einsum('ni,nij->nj', coefficients, node_rest[indices])-points, axis=1)
assert rest_error.max() < .001
target_rest_error = np.linalg.norm(np.einsum('ni,nij->nj', target_coefficients, target_rest[target_indices])-points, axis=1)
assert target_rest_error.max() < .001
empty = np.asarray([any(not nodes[lookup[tuple(cell+(x,y,z))]]['weights'] for x in (0,1) for y in (0,1) for z in (0,1)) for cell in cells])
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@local if bone['parent'] >= 0 else local)

def influences(rows):
    vi, bi, weights = [], [], []
    for i, row in enumerate(rows):
        for name, weight in row:
            vi.append(i); bi.append(names[name]); weights.append(weight)
    return np.asarray(vi, dtype=int), np.asarray(bi, dtype=int), np.asarray(weights)

body_inf = influences(body['weights'])
old_inf = influences([n['weights'] for n in nodes])
new_inf = influences([n['weights'] for n in revised_nodes])
arm = np.asarray([sum(weight for name, weight in row if any(s in name for s in ('arm','hand','thumb','index','middle','ring','pinky'))) for row in body['weights']])
roi = np.flatnonzero((points[:,2] >= 90)&(points[:,2] <= 125)&(arm < .25))
morphed = points.copy()
for morph in mesh['morph_targets']:
    if morph['name'] in ('PBMHipSize', 'PBMWaistWidth'):
        for vertex, *delta in morph['deltas']:
            morphed[vertex] += delta

def skin(rest, inf, matrices):
    vi, bi, weight = inf
    result = np.zeros_like(rest)
    np.add.at(result, vi, (np.einsum('nij,nj->ni', matrices[bi,:3,:3], rest[vi])+matrices[bi,:3,3])*weight[:,None])
    return result

def stats(error):
    region = error[roi]
    return {'whole_body_max_cm': float(error.max()), 'skirt_max_cm': float(region.max()),
            'whole_body_worst_index':int(error.argmax()),'skirt_worst_index':int(roi[region.argmax()]),
            'skirt_p95_cm': float(np.percentile(region,95)), 'skirt_over_3mm': int((region > .3).sum()),
            'skirt_over_1cm': int((region > 1).sum())}

rows, native_error = [], None
native_check = json.loads((w/'ls128-map68.json').read_text())['bodies'][0]
for clip in ('sprint', 'walk', 'jog'):
    motion = json.loads((w/f'follow-{clip}-base.json').read_text())
    for frame_index, frame in enumerate(motion['frames']):
        snapshot = frame['pose']['Snapshot']
        recorded = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
        pose = []
        for bone in mesh['bones']:
            t = recorded[bone['name']]
            assert max(abs(t['Scale3D'][k]-1) for k in 'XYZ') < .0001
            local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']), Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector((1,1,1)))
            pose.append(pose[bone['parent']]@local if bone['parent'] >= 0 else local)
        matrices = np.asarray([np.asarray(p@b.inverted()) for p,b in zip(pose,bind,strict=True)])
        old_nodes = skin(node_rest, old_inf, matrices)
        unweighted = np.asarray([not n['weights'] for n in nodes])
        old_nodes[unweighted] = node_rest[unweighted]@matrices[root_index,:3,:3].T+matrices[root_index,:3,3]
        new_nodes = skin(target_rest, new_inf, matrices)
        original = np.einsum('ni,nij->nj', coefficients, old_nodes[indices])
        original[empty] = points[empty]@matrices[root_index,:3,:3].T+matrices[root_index,:3,3]
        revised = np.einsum('ni,nij->nj', target_coefficients, new_nodes[target_indices])
        if a.surface_frame == f'{clip}:{frame_index}':
            expected_surface = skin(points, body_inf, matrices)
            mapped_tree = BVHTree.FromPolygons(revised.tolist(), body['indices'], all_triangles=True)
            nearest = [mapped_tree.find_nearest(Vector(point)) for point in expected_surface]
            distances = np.asarray([row[3] for row in nearest])
            correspondence = np.linalg.norm(revised-expected_surface, axis=1)
            worst = int(roi[correspondence[roi].argmax()])
            surface = {'scope':'Approximate body triangulation mapped through the lattice. Unsigned nearest distance, not native SDF or containment.',
                       'clip':clip,'frame':frame_index,'body_positions_cm':expected_surface.tolist(),
                       'mapped_positions_cm':revised.tolist(),'nearest_distance_cm':distances.tolist(),
                       'correspondence':stats(correspondence),'nearest':stats(distances),
                       'worst_correspondence':{'index':worst,'rest_cm':points[worst].tolist(),
                           'body_weights':body['weights'][worst],
                           'nearest_distance_cm':float(distances[worst]),
                           'corners':[{'coefficient':float(coefficient),'node':revised_nodes[index]}
                                      for index,coefficient in zip(target_indices[worst],target_coefficients[worst],strict=True)]}}
            (a.output/'surface.json').write_text(json.dumps(surface,separators=(',', ':'))+'\n')
        if clip == 'sprint' and frame_index == 8:
            differences = np.linalg.norm(original-np.asarray(native_check['sample_lattice_positions_cm']),axis=1)
            native_error = float(differences.max())
            replay = {'max_cm':native_error,'worst_body_index':int(differences.argmax()),
                      'skirt_max_cm':float(differences[roi].max()),'p95_cm':float(np.percentile(differences,95)),
                      'original_cm':original[differences.argmax()].tolist(),
                      'native_cm':native_check['sample_lattice_positions_cm'][int(differences.argmax())]}
            (a.output/'native-replay.json').write_text(json.dumps(replay,indent=2)+'\n')
            # Blender's float matrices and native double transforms are compared below 0.1 mm,
            # with a tighter 0.01 mm check in the skirt region used for fitting decisions.
            assert native_error < .01 and replay['skirt_max_cm'] < .001, replay
        for label, target in [('default', points), ('hip-waist', morphed)]:
            expected = skin(target, body_inf, matrices)
            old_error = np.linalg.norm(original-expected,axis=1)
            new_error = np.linalg.norm(revised-expected,axis=1)
            rows.append({'clip':clip,'frame':frame_index,'case':label,'original':stats(old_error),'local_transfer':stats(new_error)})
        if frame_index % 32 == 0:
            print(clip,frame_index,rows[-2]['original']['skirt_max_cm'],rows[-2]['local_transfer']['skirt_max_cm'],flush=True)
assert native_error is not None
if a.surface_frame:
    assert (a.output/'surface.json').is_file(), f'Unknown recorded frame: {a.surface_frame}'
report = {'scope':'Offline fixed-domain mapping comparison across all recorded locomotion poses, default and combined hip/waist. The collider stays unmorphed. No signed-distance query, native cloth simulation, cost or garment acceptance.',
          'source_node_count':len(nodes),'node_count':len(target_nodes),'subdivision':a.subdivide,
          'body_points':len(points),'skirt_samples':len(roi),'max_pruned_weight':max_dropped,
          'clamped_barycentric_nodes':clamped_count,'closest_point_reconstruction_max_cm':max_closest_error,
          'rest_reconstruction_max_cm':float(rest_error.max()),'target_rest_reconstruction_max_cm':float(target_rest_error.max()),
          'native_frame68_replay_max_cm':native_error,'cases':rows}
(a.output/'motion.json').write_text(json.dumps(report,indent=2)+'\n')
print('completed',len(rows),'cases; native replay',native_error,flush=True)

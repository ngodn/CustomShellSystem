"""Cut overlapping closed collision regions while retaining source-point provenance."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import bmesh
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.output.mkdir(exist_ok=False)
w = Path(__file__).resolve().parents[2]/'work/eve26'
source = w/'body-collider.json'
body = json.loads(source.read_text())
points = np.asarray(body['positions'])
definitions = [('pelvis', (-24, 24, 98, 136)), ('thigh_l', (-2, 24, 75, 110)), ('thigh_r', (-24, 2, 75, 110))]
reports = []
for name, (xmin, xmax, zmin, zmax) in definitions:
    bm = bmesh.new()
    provenance = bm.verts.layers.deform.new('source_points')
    verts = []
    for i, point in enumerate(points):
        vertex = bm.verts.new(point)
        vertex[provenance][i] = 1.
        verts.append(vertex)
    for face in body['indices']:
        bm.faces.new([verts[i] for i in reversed(face)])
    original_boundary = sum(e.is_boundary for e in bm.edges)
    cap_records = []
    for axis, bound, sign in ((0, xmin, -1), (0, xmax, 1), (2, zmin, -1), (2, zmax, 1)):
        co, normal = [0., 0., 0.], [0., 0., 0.]
        co[axis], normal[axis] = bound, sign
        bmesh.ops.bisect_plane(bm, geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
                              dist=1e-6, plane_co=co, plane_no=normal, clear_outer=True)
        boundary = [e for e in bm.edges if e.is_boundary and all(abs(v.co[axis]-bound) < 1e-4 for v in e.verts)]
        caps = bmesh.ops.holes_fill(bm, edges=boundary, sides=0)['faces'] if boundary else []
        cap_records.append({'axis': axis, 'bound_cm': bound, 'boundary_edges': len(boundary), 'cap_faces': len(caps)})
    remaining_boundary = [e for e in bm.edges if e.is_boundary]
    extra_caps = bmesh.ops.holes_fill(bm, edges=remaining_boundary, sides=0)['faces'] if remaining_boundary else []
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    pending = set(bm.faces)
    components = []
    while pending:
        connected, stack = set(), [pending.pop()]
        while stack:
            face = stack.pop()
            connected.add(face)
            for edge in face.edges:
                for neighbour in edge.link_faces:
                    if neighbour in pending:
                        pending.remove(neighbour)
                        stack.append(neighbour)
        volume = sum(f.verts[0].co.dot(f.verts[1].co.cross(f.verts[2].co))/6 for f in connected)
        components.append((volume, connected))
    components.sort(key=lambda row: abs(row[0]), reverse=True)
    assert components[0][0] > 0 and components[0][0] > .9*sum(abs(volume) for volume, _ in components)
    discarded = [{'triangles': len(faces), 'volume_cm3': volume} for volume, faces in components[1:]]
    if discarded:
        bmesh.ops.delete(bm, geom=[face for _, faces in components[1:] for face in faces], context='FACES')
    used = {v for face in bm.faces for v in face.verts}
    vertices = sorted(used, key=lambda v: tuple(v.co))
    lookup = {v: i for i, v in enumerate(vertices)}
    xyz, weights, transfers = [], [], []
    errors = []
    for v in vertices:
        transfer = {int(i): float(amount) for i, amount in v[provenance].items() if amount > 1e-9}
        total = sum(transfer.values())
        assert abs(total-1) < 1e-5, (name, total)
        transfer = {i: amount/total for i, amount in transfer.items()}
        point = np.asarray(v.co)
        reconstructed = sum((amount*points[i] for i, amount in transfer.items()), np.zeros(3))
        errors.append(float(np.linalg.norm(point-reconstructed)))
        merged = {}
        for i, amount in transfer.items():
            for bone, weight in body['weights'][i]:
                merged[bone] = merged.get(bone, 0.)+amount*weight
        assert abs(sum(merged.values())-1) < 1e-5
        xyz.append(point.tolist())
        weights.append(sorted(merged.items()))
        transfers.append(sorted(transfer.items()))
    invalid = sum(not e.is_manifold for e in bm.edges if any(v in used for v in e.verts) and e.link_faces)
    report = {'region': name, 'bounds_cm': [xmin, xmax, zmin, zmax], 'original_boundary_edges': original_boundary,
              'cuts': cap_records, 'extra_caps': len(extra_caps), 'vertices': len(vertices), 'triangles': len(bm.faces),
              'discarded_disconnected_components': discarded,
              'nonmanifold_surface_edges': invalid, 'max_provenance_error_cm': max(errors),
              'max_bone_influences': max(map(len, weights)), 'volume_cm3': bm.calc_volume(signed=True)}
    (a.output/(name+'.json')).write_text(json.dumps({'scope': 'Derived closed regional surface with source-point provenance and interpolated source skin weights. No source edit, native asset or acceptance.',
        'bone': name, 'positions': xyz, 'weights': weights, 'source_transfer': transfers,
        'indices': [[lookup[v] for v in face.verts] for face in bm.faces], 'report': report}, separators=(',', ':'))+'\n')
    reports.append(report)
    print(report, flush=True)
    bm.free()
(a.output/'receipt.json').write_text(json.dumps({'source': str(source), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'regions': reports}, indent=2)+'\n')
assert all(r['nonmanifold_surface_edges'] == 0 and r['max_provenance_error_cm'] < .0001 and r['volume_cm3'] > 0 for r in reports), 'Inspect region closure/provenance before further use'

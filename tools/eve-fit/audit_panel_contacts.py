"""Measure worst panel edges against the private collider recipe in recorded poses."""
import argparse
import json
import math
import sys
from pathlib import Path

from mathutils import Matrix, Quaternion, Vector

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2] / 'work/eve26'
mesh = json.loads((w / 'holiday.mesh.json').read_text())
motion = json.loads((w / 'follow-sprint-base.json').read_text())
recipe = json.loads((w / 'cloth-capsules.json').read_text())
edges = json.loads((w / 'panel-motion2-edges.json').read_text())
no_collision = json.loads((w / 'panel-nocoll.json').read_text())
connected = {i for c in recipe['connections'] for i in c['sphere_indices']}

def swept_sphere_phi(point, start, end, r0, r1):
    axis = end - start
    length = axis.length
    if length < 1e-8 or abs(r1-r0) >= length:
        return min((point-start).length-r0, (point-end).length-r1)
    axis /= length
    relative = point-start
    axial = relative.dot(axis)
    radial = (relative-axis*axial).length
    slope = (r1-r0)/length
    along = min(length, max(0., axial+slope*radial/math.sqrt(1.-slope*slope)))
    return math.hypot(axial-along, radial)-r0-slope*along

rows = []
for case in edges['cases']:
    frame = case['frame']
    snap = motion['frames'][frame]['pose']['Snapshot']
    entries = dict(zip(snap['BoneNames'], snap['LocalTransforms'], strict=True))
    transforms = []
    by_name = {}
    for bone in mesh['bones']:
        e = entries[bone['name']]
        scale = [e['Scale3D'][k] for k in 'XYZ']
        assert max(abs(v-1.) for v in scale) < 1e-4, 'Collider radius scaling needs explicit handling'
        local = Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),
            Quaternion([e['Rotation'][k] for k in 'WXYZ']), Vector(scale))
        transform = transforms[bone['parent']] @ local if bone['parent'] >= 0 else local
        transforms.append(transform)
        by_name[bone['name']] = transform
    spheres = [(by_name[s['bone']] @ Vector(s['local_center_cm']), s['radius_cm']) for s in recipe['spheres']]
    def contacts(point):
        hits = []
        for index, c in enumerate(recipe['connections']):
            i, j = c['sphere_indices']
            phi = swept_sphere_phi(point, spheres[i][0], spheres[j][0], spheres[i][1], spheres[j][1])
            if phi < .3:
                hits.append({'capsule':index, 'bone':c['bone'], 'phi_cm':phi})
        for index, (center, radius) in enumerate(spheres):
            if index in connected:
                continue
            phi = (point-center).length-radius
            if phi < .3:
                hits.append({'sphere':index, 'bone':recipe['spheres'][index]['bone'], 'phi_cm':phi})
        return sorted(hits, key=lambda h:h['phi_cm'])
    edge_rows = []
    for edge in case['worst_edges']:
        endpoints = []
        for j, vertex in enumerate(edge['vertices']):
            endpoints.append({'vertex':vertex, 'max_distance_cm':edge['max_distance_cm'][j],
                'skinned_contacts':contacts(Vector(edge['skin_positions_cm'][j])),
                'simulated_contacts':contacts(Vector(edge['actual_positions_cm'][j])),
                'no_collision_contacts':contacts(Vector(no_collision['frames'][frame]['positions_cm'][vertex]))})
        edge_rows.append({'vertices':edge['vertices'], 'ratio':edge['ratio'], 'endpoints':endpoints})
    rows.append({'frame':frame, 'edges':edge_rows})
a.output.write_text(json.dumps({'scope':'Analytic swept-sphere recipe check in recorded bone poses, with 0.3 cm contact margin. Recipe was separately checked against saved PA_Holiday. Not exported solver collision state, continuous contact or fitting acceptance.', 'frames':rows}, indent=2)+'\n')
for row in rows:
    e = row['edges'][0]
    print(row['frame'], e['vertices'], [[(h.get('bone'), round(h['phi_cm'], 3)) for h in p['skinned_contacts']] for p in e['endpoints']])

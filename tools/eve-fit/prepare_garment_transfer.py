"""Test local body-surface weights on the lower dress without changing its shape."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.output.mkdir(exist_ok=False)
w = Path(__file__).resolve().parents[2]/'work/eve26'
source = w/'holiday-waist-source.mesh.json'
mesh = json.loads(source.read_text())
body = json.loads((w/'body-collider.json').read_text())
points = np.asarray(body['positions'])
names = {bone['name']: i for i, bone in enumerate(mesh['bones'])}
arm = [sum(weight for name, weight in row if any(part in name for part in
       ('arm', 'hand', 'thumb', 'index', 'middle', 'ring', 'pinky'))) for row in body['weights']]
faces = [face for face in body['indices'] if not any(arm[v] > .25 for v in face)]
tree = BVHTree.FromPolygons(points.tolist(), faces, all_triangles=True)
materials = {
    'MI_CH_P_EVE_Christmas_01_01.001', 'MI_CH_P_EVE_Christmas_01_Decal.001',
    'MI_EVE_HR_Christmas_01_Fur.001', 'MI_EVE_HR_15_Emissive1.001'}
slots = {i for i, name in enumerate(mesh['materials']) if name in materials}
selected = {mesh['wedges'][wedge][0] for face in mesh['faces'] if face[3] in slots
            for wedge in face[:3] if mesh['points'][mesh['wedges'][wedge][0]][2] < 112}
original = {}
for vertex, bone, weight in mesh['influences']:
    original.setdefault(vertex, {})[bone] = weight
revised = {}
records = []
for vertex in sorted(selected):
    point = np.asarray(mesh['points'][vertex])
    closest, _, face_id, distance = tree.find_nearest(Vector(point))
    face = faces[face_id]
    triangle = points[face]
    uv = np.linalg.lstsq(np.column_stack((triangle[1]-triangle[0], triangle[2]-triangle[0])),
                         np.asarray(closest)-triangle[0], rcond=None)[0]
    bary = np.clip([1-uv.sum(), *uv], 0, 1)
    bary /= bary.sum()
    assert np.linalg.norm(bary@triangle-np.asarray(closest)) < .0001
    transferred = {}
    for v, amount in zip(face, bary, strict=True):
        for name, weight in body['weights'][v]:
            bone = names[name]
            transferred[bone] = transferred.get(bone, 0.)+float(amount)*weight
    t = float(np.clip((112-point[2])/12, 0, 1))
    alpha = t*t*(3-2*t)
    weights = {bone: (1-alpha)*original[vertex].get(bone, 0.)+alpha*transferred.get(bone, 0.)
               for bone in original[vertex].keys() | transferred.keys()}
    ordered = sorted(((b, v) for b, v in weights.items() if v > 1e-8), key=lambda row: -row[1])
    dropped = sum(value for _, value in ordered[12:])
    assert dropped < .001, (vertex, dropped)
    total = sum(value for _, value in ordered[:12])
    revised[vertex] = {bone: value/total for bone, value in ordered[:12]}
    records.append({'vertex': vertex, 'alpha': alpha, 'body_face': face, 'distance_cm': distance,
                    'dropped_weight': dropped, 'original': original[vertex], 'revised': revised[vertex]})
preserved = hashlib.sha256(json.dumps({k: v for k, v in mesh.items() if k != 'influences'},
                                    sort_keys=True).encode()).hexdigest()
mesh['influences'] = [[vertex, bone, value] for vertex, weights in original.items()
                      for bone, value in revised.get(vertex, weights).items()]
assert preserved == hashlib.sha256(json.dumps({k: v for k, v in mesh.items() if k != 'influences'},
                                             sort_keys=True).encode()).hexdigest()
assert all(vertex >= len(points) for vertex in revised), 'Body source indices must remain untouched'
(a.output/'candidate.mesh.json').write_text(json.dumps(mesh, separators=(',', ':'))+'\n')
(a.output/'transfer.json').write_text(json.dumps({
    'scope': 'Offline lower-garment weight hypothesis, not accepted. Full transfer below 100 cm, smooth fade to original at 112 cm. No geometry, body, morph or skeleton changes.',
    'source': str(source), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'non_weight_sha256': preserved, 'vertices': len(revised), 'records': records}, indent=2)+'\n')
print('Prepared lower-garment transfer for', len(revised), 'vertices', flush=True)

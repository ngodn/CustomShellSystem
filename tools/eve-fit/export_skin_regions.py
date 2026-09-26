"""Serialize derived collision regions for isolated CSSImportMesh imports."""
import argparse
import hashlib
import json
import math
from collections import Counter
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--regions', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--only', choices=('pelvis', 'thigh_l', 'thigh_r'))
p.add_argument('--weld', type=float, default=0., help='Derived collision copy only, maximum weld distance in cm')
a = p.parse_args()
assert 0 <= a.weld <= .001
a.output.mkdir(exist_ok=False)
w = Path(__file__).resolve().parents[2] / 'work/eve26'
source_path = w / 'holiday-waist-source.mesh.json'
source = json.loads(source_path.read_text())
body_path = w / 'body-collider.json'
body = json.loads(body_path.read_text())
receipt = json.loads((a.regions / 'receipt.json').read_text())
assert hashlib.sha256(body_path.read_bytes()).hexdigest() == receipt['source_sha256']
assert source['points'][:len(body['positions'])] == body['positions']
names = {b['name']: i for i, b in enumerate(source['bones'])}
reports = []
for region, suffix in (('pelvis', 'Pelv'), ('thigh_l', 'ThighL'), ('thigh_r', 'ThighR')):
    if a.only and region != a.only:
        continue
    path = a.regions / (region + '.json')
    data = json.loads(path.read_text())
    assert data['report']['nonmanifold_surface_edges'] == 0
    welded, max_shift = 0, 0.
    if a.weld:
        kept, remap = [], {}
        for i, point in enumerate(data['positions']):
            match = next((j for j in kept if math.dist(point, data['positions'][j]) <= a.weld), None)
            if match is None:
                kept.append(i)
                match = i
            else:
                welded += 1
                max_shift = max(max_shift, math.dist(point, data['positions'][match]))
            remap[i] = match
        compact = {old: new for new, old in enumerate(kept)}
        faces = [[compact[remap[i]] for i in face] for face in data['indices']]
        data['indices'] = [f for f in faces if len(set(f)) == 3]
        assert len({tuple(sorted(f)) for f in data['indices']}) == len(data['indices'])
        edges = Counter(tuple(sorted((face[i], face[(i+1)%3]))) for face in data['indices'] for i in range(3))
        assert set(edges.values()) == {2}, 'Weld must preserve the closed surface'
        for key in ('positions', 'weights', 'source_transfer'):
            data[key] = [data[key][i] for i in kept]
    points = data['positions']
    out = dict(schema=1, uv_channels=1, bones=source['bones'], points=points,
               mesh_package='/Game/CSS/EveTest/SK_C' + suffix,
               skeleton_package='/Game/CSS/EveTest/SKEL_C' + suffix,
               materials=['Collision'], wedges=[], normals=[], faces=[], influences=[], morph_targets=[])
    for face in data['indices']:
        x, y, z = [points[i] for i in face]
        u, v = [[b-a for a, b in zip(x, other)] for other in (y, z)]
        cross = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
        length = math.sqrt(sum(n*n for n in cross))
        assert length*length >= 1e-12, (region, face, 'Below the native import triangle threshold')
        normal = [n/length for n in cross]
        start = len(out['wedges'])
        # The existing UE import uses the opposite winding from the outward BMesh surface.
        for i, uv in zip(reversed(face), ((0., 0.), (1., 0.), (0., 1.)), strict=True):
            out['wedges'].append([i, *uv])
            out['normals'].append(normal)
        out['faces'].append([start, start+1, start+2, 0])
    for i, weights in enumerate(data['weights']):
        assert len(weights) <= 8 and abs(sum(v for _, v in weights)-1) < 1e-5
        out['influences'].extend([i, names[name], amount] for name, amount in weights if amount > 0)
    empty_morphs = []
    for morph in source['morph_targets']:
        lookup = {row[0]: row[1:] for row in morph['deltas']}
        deltas = []
        for i, transfer in enumerate(data['source_transfer']):
            delta = [sum(amount*lookup.get(index, (0., 0., 0.))[axis] for index, amount in transfer) for axis in range(3)]
            if any(abs(v) > 1e-9 for v in delta):
                deltas.append([i, *delta])
        if deltas:
            out['morph_targets'].append(dict(name=morph['name'], deltas=deltas))
        else:
            empty_morphs.append(morph['name'])
    target = a.output / (region + '.mesh.json')
    target.write_text(json.dumps(out, separators=(',', ':'))+'\n')
    reports.append(dict(region=region, source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                        output_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
                        points=len(points), faces=len(out['faces']), bones=len(out['bones']),
                        welded_vertices=welded, maximum_weld_shift_cm=max_shift,
                        morphs=len(out['morph_targets']), empty_morphs_omitted=empty_morphs,
                        used_bones=sorted({name for weights in data['weights'] for name, amount in weights if amount > 0})))
(a.output / 'receipt.json').write_text(json.dumps(dict(
    scope='Private collision-authoring imports only. Original skeleton definitions retained; no production replacement. Morph import does not prove native collision morph support.',
    source_sha256=hashlib.sha256(source_path.read_bytes()).hexdigest(), regions=reports), indent=2)+'\n')
print(json.dumps(reports, indent=2))

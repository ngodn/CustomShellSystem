"""Prepare a bounded Alice bow trial using the author's attachment weights."""
import argparse
import hashlib
import json
import math
from pathlib import Path

work = Path(__file__).resolve().parents[2] / 'work/eve26'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repaired', action='store_true')
args = parser.parse_args()
revision = 2 if args.repaired else 1
source = work / ('alice-import4/alice.mesh.json' if args.repaired else 'alice-import1/alice.mesh.json')
data = json.loads(source.read_text())
audit = json.loads(source.with_suffix('.audit.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest() == audit['output_sha256']
part = audit['parts'][2]
assert part['name'] == 'Eve Midsummer Alice - Ribbon'
start = sum(p['points'] for p in audit['parts'][:2])
first = sum(p['faces'] for p in audit['parts'][:2])
points = data['points'][start:start+part['points']]
faces = data['faces'][first:first+part['faces']]
author_path = work / 'alice-author-motion2.json'
author = next(o for o in json.loads(author_path.read_text())['objects']
              if o['name'] == part['name'])
error = max(math.dist([p[0]*100, -p[1]*100, p[2]*100], q)
            for p, q in zip(author['points_world'], points, strict=True))
assert error < .0005
moving = [0.] * len(points)
root = [0.] * len(points)
for i, name, weight in author['weights']:
    if name == 'Ab-Fr-Neck-RibbonRoot': root[i] += weight
    elif name != 'Bip001-Neck': moving[i] += weight
limits = [m/(m+r) if m > 1e-7 else 0. for m, r in zip(moving, root)]
normals = [[0., 0., 0.] for _ in points]
triangles = []
neighbors = [set() for _ in points]
for face in faces:
    ids = [data['wedges'][w][0]-start for w in face[:3]]
    triangles.extend(ids)
    for i, wedge in zip(ids, face[:3]):
        neighbors[i].update(ids)
        for axis in range(3): normals[i][axis] += data['normals'][wedge][axis]
for normal in normals:
    length = math.sqrt(sum(x*x for x in normal))
    assert length > 1e-7
    for axis in range(3): normal[axis] /= length
remaining = set(range(len(points)))
components = []
while remaining:
    stack = [remaining.pop()]
    group = []
    while stack:
        i = stack.pop(); group.append(i)
        new = neighbors[i] & remaining
        remaining -= new; stack.extend(new)
    pins = sum(limits[i] == 0 for i in group)
    assert pins > 0, 'Every disconnected bow piece must remain attached'
    components.append({'vertices': len(group), 'pins': pins})
weights = [[] for _ in points]
for i, bone, weight in data['influences']:
    if start <= i < start+len(points): weights[i-start].append([data['bones'][bone]['name'], weight])
slot = dict(positions=points, normals=normals, indices=triangles, weights=weights,
            max_distances=limits, backstop_distances=[0.]*len(points),
            backstop_radii=[5. if d > 0 else 0. for d in limits])
out = work / f'alice-cloth{revision}'
out.mkdir(exist_ok=False)
def save(name, value): (out/name).write_text(json.dumps(value, separators=(',', ':'))+'\n')
save('proxy.json', {'slots': {'AliceRibbon': slot}})
save('config.json', {'AliceRibbon': dict(Iterations=6, BendingStiffness=.5,
    AnimDriveStiffness=.35, AnimDriveDamping=.5, DampingCoefficient=.3,
    CollisionThickness=.1, FrictionCoefficient=.2, GravityScale=1., SelfCollision=0)})
mesh = '/Game/CSS/EveTest/SK_AFit4' if args.repaired else '/Game/CSS/EveTest/SK_AFit1'
physics = '/Game/CSS/SeduXtress/PA_Body'
save('copy.json', {'mapping': {mesh:f'/Game/CSS/EveTest/SK_ACloth{revision}'}, 'copy':[mesh]})
save('collision-copy.json', {'mapping': {physics:f'/Game/CSS/EveTest/PA_ACloth{revision}'}, 'copy':[physics]})
save('receipt.json', dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    author_sha256=hashlib.sha256(author_path.read_bytes()).hexdigest(),
    point_error_cm=error, components=components, max_distance_cm=max(limits),
    scope='Unverified bow trial. Author chain/root proportions bound displacement to 1 cm; not a conversion of the original solver. Native motion and visual checks required.'))
print(out)

"""Extract Holiday's main connected fabric panel as a simulation-surface candidate."""
import json
import math
import argparse
from collections import defaultdict
from pathlib import Path

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesh', type=Path, default=WORK/'holiday.mesh.json')
parser.add_argument('--name', default='skirt-surface')
args = parser.parse_args()
assert args.name.replace('-', '').isalnum()
output = WORK / f'{args.name}.json'
assert not output.exists()
data = json.loads(args.mesh.read_text())
slot = data['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
triangles = [[data['wedges'][w][0] for w in f[:3]] for f in data['faces'] if f[3] == slot]
neighbors = defaultdict(set)
for tri in triangles:
    for i in range(3):
        a, b = tri[i], tri[(i + 1) % 3]
        neighbors[a].add(b)
        neighbors[b].add(a)
seen = set()
components = []
for start in neighbors:
    if start in seen:
        continue
    group = [start]
    seen.add(start)
    for vertex in group:
        for other in neighbors[vertex]:
            if other not in seen:
                seen.add(other)
                group.append(other)
    components.append(group)
selected = sorted(max(components, key=len))
assert len(selected) == 4336, 'Export changed; review panel selection'
lookup = {old: new for new, old in enumerate(selected)}
faces = [[lookup[v] for v in tri] for tri in triangles if tri[0] in lookup]
positions = [data['points'][v] for v in selected]
normals = [[0., 0., 0.] for _ in positions]
edges = defaultdict(int)
for face in faces:
    a, b, c = [positions[i] for i in face]
    u, v = [b[i]-a[i] for i in range(3)], [c[i]-a[i] for i in range(3)]
    normal = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
    assert math.sqrt(sum(x*x for x in normal)) > 1e-8
    for i in range(3):
        edges[tuple(sorted((face[i], face[(i+1)%3])))] += 1
# UE clockwise indices have the opposite cross-product orientation from the
# exported surface normals. Preserve the measured normals instead.
for face in data['faces']:
    if face[3] != slot or data['wedges'][face[0]][0] not in lookup:
        continue
    for wedge in face[:3]:
        i = lookup[data['wedges'][wedge][0]]
        normals[i] = [normals[i][j]+data['normals'][wedge][j] for j in range(3)]
for i, normal in enumerate(normals):
    size = math.sqrt(sum(x*x for x in normal))
    assert size > 1e-8
    normals[i] = [x/size for x in normal]
weights = [[] for _ in selected]
for vertex, bone, weight in data['influences']:
    if vertex in lookup:
        weights[lookup[vertex]].append([data['bones'][bone]['name'], weight])
assert all(w and len(w) <= 8 and abs(sum(v for _, v in w)-1) < 1e-5 for w in weights)
output.write_text(json.dumps({'positions': positions, 'normals': normals,
    'source_mesh':str(args.mesh),'source_vertices':selected,
    'weights': weights, 'indices': [i for face in faces for i in face]}, separators=(',', ':')))
report = {'vertices': len(positions), 'triangles': len(faces),
    'boundary_edges': sum(n == 1 for n in edges.values()),
    'nonmanifold_edges': sum(n > 2 for n in edges.values()),
    'z_cm': [min(p[2] for p in positions), max(p[2] for p in positions)],
    'stage': 'Undecimated connected surface with original weights; anchors, weight retargeting and render attachments pending'}
(WORK / f'{args.name}.audit.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))

"""Create a bounded outer-dress offset around the measured underwear intersection."""
import heapq
import json
import math
from collections import defaultdict
from pathlib import Path

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
data = json.loads((WORK/'holiday.mesh.json').read_text())
measured = json.loads((WORK/'hip-patch-nearest.json').read_text())
seed = {i for c in measured['cases'] for hit in c['hits'] for i in hit['nearest_cloth_vertices']}
slot = data['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
neighbors = defaultdict(set)
normals = defaultdict(lambda: [0., 0., 0.])
for f in data['faces']:
    if f[3] != slot:
        continue
    indices = [data['wedges'][w][0] for w in f[:3]]
    for j, w in enumerate(f[:3]):
        i = indices[j]
        normals[i] = [a+b for a, b in zip(normals[i], data['normals'][w])]
        neighbors[i].update((indices[(j+1)%3], indices[(j+2)%3]))
assert seed <= neighbors.keys()
distance = {i: 0. for i in seed}
queue = [(0., i) for i in seed]
heapq.heapify(queue)
while queue:
    d, i = heapq.heappop(queue)
    if d != distance[i]:
        continue
    for j in neighbors[i]:
        step = math.dist(data['points'][i], data['points'][j])
        trial = d+step
        if trial < 3. and trial < distance.get(j, float('inf')):
            distance[j] = trial
            heapq.heappush(queue, (trial, j))
offsets = []
for i, d in sorted(distance.items()):
    blend = min(1., max(0., (3.-d)/2.2))
    amplitude = .25*blend*blend*(3.-2.*blend)
    length = math.sqrt(sum(v*v for v in normals[i]))
    assert length > 1e-8
    delta = [v/length*amplitude for v in normals[i]]
    offsets.append([i, *delta])
    data['points'][i] = [a+b for a, b in zip(data['points'][i], delta)]
data['mesh_package'] = '/Game/CSS/EveTest/SK_HolidayHip'
out = WORK/'holiday-hip.mesh.json'
assert not out.exists()
out.write_text(json.dumps(data, separators=(',', ':')))
(WORK/'hip-offsets.json').write_text(json.dumps({'offsets': offsets, 'max_offset_cm': .25,
    'seed_vertices': len(seed), 'scope': 'Outer dress only. Same base offset also moves all relative morph endpoints; no body edits. Export normals await source re-export.'}, indent=2)+'\n')
print('Prepared', len(offsets), 'outer-dress offsets, at most 2.5 mm')

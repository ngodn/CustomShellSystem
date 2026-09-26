"""Make a bounded garment-only waist fit trial from traced torso intersections."""
import heapq
import json
import math
import argparse
import sys
from collections import defaultdict
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree

w = Path(__file__).resolve().parents[2]/'work/eve26'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--receipt', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
output = a.output
assert not output.exists()
assert not a.receipt.exists()
mesh = json.loads((w/'holiday-hip-clean.mesh.json').read_text())
body = json.loads((w/'body-collider.json').read_text())
body_tree = BVHTree.FromPolygons(body['positions'], [[c, b, a] for a, b, c in body['indices']], all_triangles=True)
trace = json.loads((w/'panel-gap-traced.json').read_text())
seeds = {r['source_vertex'] for r in trace['clipped_mapped_vertices']
         if 120 <= r['rest_cm'][2] < 130 and r['source_match_cm'] < .001}
assert seeds
slot = mesh['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
neighbors = defaultdict(set)
normals = defaultdict(lambda: [0., 0., 0.])
for face in mesh['faces']:
    if face[3] != slot:
        continue
    ids = [mesh['wedges'][i][0] for i in face[:3]]
    for j, wedge in enumerate(face[:3]):
        v = ids[j]
        normals[v] = [a+b for a, b in zip(normals[v], mesh['normals'][wedge])]
        neighbors[v].update((ids[(j+1)%3], ids[(j+2)%3]))
assert seeds <= neighbors.keys()
distance = {i: 0. for i in seeds}
queue = [(0., i) for i in seeds]
heapq.heapify(queue)
while queue:
    d, i = heapq.heappop(queue)
    if d != distance[i]:
        continue
    for j in neighbors[i]:
        trial = d+math.dist(mesh['points'][i], mesh['points'][j])
        if trial < 2.5 and trial < distance.get(j, float('inf')):
            distance[j] = trial
            heapq.heappush(queue, (trial, j))
offsets = []
flipped = 0
for i, d in sorted(distance.items()):
    blend = min(1., max(0., (2.5-d)/2.))
    amplitude = .5*blend*blend*(3.-2.*blend)
    length = math.sqrt(sum(n*n for n in normals[i]))
    assert length > 1e-8
    delta = [n/length*amplitude for n in normals[i]]
    _, outward, _, _ = body_tree.find_nearest(Vector(mesh['points'][i]))
    if Vector(delta).dot(outward) < 0:
        delta = [-n for n in delta]
        flipped += 1
    offsets.append([i, *delta])
    mesh['points'][i] = [a+b for a, b in zip(mesh['points'][i], delta)]
mesh['mesh_package'] = '/Game/CSS/EveTest/SK_Waist'
output.write_text(json.dumps(mesh, separators=(',', ':'))+'\n')
a.receipt.write_text(json.dumps({'source': 'holiday-hip-clean.mesh.json', 'normals_flipped_to_body': flipped,
    'seeds': sorted(seeds), 'offsets': offsets, 'max_cm': .5,
    'scope': 'Local base-geometry trial only. Body, weights, topology and relative morph deltas unchanged. Requires pose/morph and source Blender verification; normals await source re-export.'}, indent=2)+'\n')
print(len(seeds), 'seeds;', len(offsets), 'garment vertices, maximum 0.5 cm')

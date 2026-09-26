"""Export the preserved body surface for a transient native cloth collision test."""
import json
from pathlib import Path

w = Path(__file__).resolve().parents[2]/'work/eve26'
m = json.loads((w/'holiday.mesh.json').read_text())
a = json.loads((w/'holiday.mesh.audit.json').read_text())
assert a['parts'][0]['name'] == 'Eve Body'
faces = [[m['wedges'][i][0] for i in f[:3]] for f in m['faces'][:a['parts'][0]['faces']]]
vertices = sorted({v for f in faces for v in f})
lookup = {v:i for i,v in enumerate(vertices)}
weights = [[] for _ in vertices]
for vertex,bone,weight in m['influences']:
    if vertex in lookup:
        weights[lookup[vertex]].append([m['bones'][bone]['name'],weight])
assert all(0<len(row)<=12 and abs(sum(w for _,w in row)-1)<1e-4 for row in weights)
out = w/'body-collider.json'
assert not out.exists()
out.write_text(json.dumps({'scope':'Unmodified old exported body only, no clothes. Full resolution feasibility control; not production performance or morph acceptance.',
    'positions':[m['points'][v] for v in vertices], 'weights':weights,
    'indices':[[lookup[v] for v in f] for f in faces], 'source_vertices':vertices})+'\n')
print(len(vertices),'vertices',len(faces),'triangles')

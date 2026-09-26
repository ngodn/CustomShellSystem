"""Measure connected tail panels and candidate upper-edge cloth anchors."""
import json
from collections import defaultdict
from pathlib import Path

work = Path(__file__).resolve().parents[2] / 'work/eve26'
out = work / 'planet-tail-panels.json'
assert not out.exists()
data = json.loads((work / 'planet-export/planet.mesh.json').read_text())
slot = data['materials'].index('PlanetTail_17')
faces = [[data['wedges'][w][0] for w in face[:3]] for face in data['faces'] if face[3] == slot]
adj = defaultdict(set)
for face in faces:
    for a,b in zip(face,face[1:]+face[:1]):
        adj[a].add(b)
        adj[b].add(a)
seen = set()
panels = []
for start in sorted(adj):
    if start in seen:
        continue
    component = [start]
    seen.add(start)
    for vertex in component:
        for neighbor in sorted(adj[vertex]):
            if neighbor not in seen:
                component.append(neighbor)
                seen.add(neighbor)
    top = max(data['points'][v][2] for v in component)
    pins = [v for v in component if data['points'][v][2] >= top-2]
    panels.append({'vertices': len(component), 'top_cm': top,
        'bottom_cm': min(data['points'][v][2] for v in component),
        'candidate_pinned_vertices': pins, 'candidate_pin_count': len(pins)})
out.write_text(json.dumps({'slot':slot,'faces':len(faces),'vertices':len(adj),'panels':panels,
    'scope':'Export topology and proposed 2 cm upper-edge pins; native cloth welding and visual anchors unverified'},indent=2)+'\n')
print(json.dumps({'faces':len(faces),'vertices':len(adj),'panels':[{k:v for k,v in p.items() if k!='candidate_pinned_vertices'} for p in panels]}))

"""Measure material-section connectivity in the exported Holiday dress (Python 3.14)."""
import json
from collections import defaultdict
from pathlib import Path

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
payload = json.loads((WORK / 'holiday.mesh.json').read_text())
audit = json.loads((WORK / 'holiday.mesh.audit.json').read_text())
offset = 0
for part in audit['parts']:
    if part['name'] == 'Eve Christmas - Dress':
        faces = payload['faces'][offset:offset + part['faces']]
        break
    offset += part['faces']
else:
    raise ValueError('Dress missing')
sections = defaultdict(list)
edge_slots = defaultdict(set)
for face in faces:
    vertices = [payload['wedges'][w][0] for w in face[:3]]
    sections[face[3]].append(vertices)
    for i in range(3):
        edge_slots[tuple(sorted((vertices[i], vertices[(i + 1) % 3])))].add(face[3])
rows = []
for slot, triangles in sections.items():
    neighbors = defaultdict(set)
    for triangle in triangles:
        for i in range(3):
            a, b = triangle[i], triangle[(i + 1) % 3]
            neighbors[a].add(b)
            neighbors[b].add(a)
    seen = set()
    components = []
    for start in neighbors:
        if start in seen:
            continue
        pending = [start]
        seen.add(start)
        for vertex in pending:
            for other in neighbors[vertex]:
                if other not in seen:
                    seen.add(other)
                    pending.append(other)
        z = [payload['points'][v][2] for v in pending]
        components.append({'vertices': len(pending), 'z_cm': [min(z), max(z)]})
    rows.append({'slot': payload['materials'][slot], 'vertices': len(neighbors),
                 'triangles': len(triangles),
                 'components': sorted(components, key=lambda row: -row['vertices'])})
boundaries = defaultdict(int)
for slots in edge_slots.values():
    if len(slots) > 1:
        boundaries[tuple(sorted(slots))] += 1
report = {'sections': rows, 'shared_material_edges': [
    {'slots': [payload['materials'][s] for s in slots], 'edges': count}
    for slots, count in boundaries.items()
], 'scope': 'Exact exported topology; coincident disconnected seams are not welded'}
output = WORK / 'holiday-sections.json'
assert not output.exists()
output.write_text(json.dumps(report, indent=2) + '\n')
for row in rows:
    print(row['slot'], row['vertices'], 'vertices,', len(row['components']), 'components')
print('Shared material boundaries:', report['shared_material_edges'])

"""Measure garment morph motion near fixed tail pins, without changing the source."""
import hashlib
import json
from pathlib import Path
import numpy as np

w = Path(__file__).resolve().parents[2] / 'work/eve26'
source = w / 'planet-f13-export/planet.mesh.json'
mesh = json.loads(source.read_text())
audit = json.loads((w / 'planet-f13-export/planet.mesh.audit.json').read_text())
proxy = json.loads((w / 'planet-tail-cloth.json').read_text())['slots']['PlanetTail_17']
points = np.asarray(mesh['points'])
starts = {}
offset = 0
for part in audit['parts']:
    starts[part['name']] = (offset, offset + part['points'])
    offset += part['points']
lo, hi = starts['Eve Prototype Planet Diving Suit - Suit']
pins = np.asarray(proxy['positions'])[:4]
nearest = np.argmin(np.linalg.norm(pins[:, None] - points[None, lo:hi], axis=2), axis=1) + lo
baseline = np.linalg.norm(points[nearest] - pins, axis=1)
rows = []
combined = np.zeros((4, 3))
for morph in mesh['morph_targets'][:6]:
    deltas = {int(d[0]): np.asarray(d[1:]) for d in morph['deltas']}
    shifts = np.asarray([deltas.get(int(i), np.zeros(3)) for i in nearest])
    combined += shifts
    rows.append(dict(morph=morph['name'], garment_motion_cm=shifts.tolist(),
                     garment_motion_length_cm=np.linalg.norm(shifts, axis=1).tolist(),
                     pin_to_same_garment_vertex_cm=np.linalg.norm(points[nearest] + shifts - pins, axis=1).tolist()))
report = dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
              nearest_garment_vertices=nearest.tolist(), baseline_distance_cm=baseline.tolist(),
              morphs=rows, combined_garment_motion_cm=combined.tolist(),
              combined_motion_length_cm=np.linalg.norm(combined, axis=1).tolist(),
              scope='Rest-pose nearest garment vertex displacement at four cloth pins. Not surface distance, visible gap, or runtime cloth morph verification.')
out = w / 'planet-f13-tail-morphs.json'
assert not out.exists()
out.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))

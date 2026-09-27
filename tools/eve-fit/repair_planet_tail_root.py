"""Trial morph propagation confined to the non-simulated tail attachment strap."""
import hashlib
import json
from pathlib import Path
import numpy as np

w = Path(__file__).resolve().parents[2] / 'work/eve26'
source = w / 'planet-f13-export/planet.mesh.json'
mesh = json.loads(source.read_text())
audit = json.loads(source.with_name('planet.mesh.audit.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest() == audit['output_sha256']
measurement = json.loads((w / 'planet-f13-tail-morphs.json').read_text())
assert measurement['source_sha256'] == audit['output_sha256']
native = json.loads((w / 'planet-f13-native.json').read_text())
section = native['render_geometry']['sections'][0]
render = np.asarray(section['positions'])
mapping = np.asarray(section['mapping']).reshape(len(render), -1, 9)
blend = (1 - mapping[:, :, 3] / 65535).mean(axis=1)
part = audit['parts'][2]
assert part['name'] == 'Eve Prototype Planet Diving Suit - Tail'
start = sum(p['points'] for p in audit['parts'][:2])
points = np.asarray(mesh['points'][start:start + part['points']])
# Leave the bottom of the pinned strap and all simulated vertices unchanged.
t = np.clip((points[:, 2] - 109.0) / 1.8, 0, 1)
falloff = t * t * (3 - 2 * t)
affected = np.flatnonzero(falloff > 0)
for index in affected:
    distances = np.linalg.norm(render - points[index], axis=1)
    matches = distances < 0.0005
    assert matches.any(), 'Missing render correspondence'
    assert (blend[matches] == 0).all(), 'Morph would affect simulated cloth'
rows = []
for measured in measurement['morphs']:
    morph = next(m for m in mesh['morph_targets'] if m['name'] == measured['morph'])
    assert not any(start <= d[0] < start + part['points'] for d in morph['deltas'])
    shift = np.asarray(measured['garment_motion_cm'][:2]).mean(axis=0)
    if np.linalg.norm(shift) < 1e-8:
        continue
    for i in affected:
        morph['deltas'].append([start + int(i), *(shift * falloff[i]).tolist()])
    morph['deltas'].sort(key=lambda d:d[0])
    rows.append(dict(morph=morph['name'], root_shift_cm=shift.tolist()))
part['exported_shapes'] = [r['morph'] for r in rows]
out = w / 'planet-fit14'
out.mkdir(exist_ok=False)
target = out / 'planet.mesh.json'
target.write_text(json.dumps(mesh, separators=(',', ':')) + '\n')
audit['output_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
(out / 'planet.mesh.audit.json').write_text(json.dumps(audit, indent=2) + '\n')
(out / 'receipt.json').write_text(json.dumps(dict(
    source_sha256=measurement['source_sha256'], output_sha256=audit['output_sha256'],
    affected_tail_points=len(affected), morphs=rows, simulated_vertices_changed=0,
    scope='Private export trial. Adds only tail strap morph deltas; rest geometry, body, suit, weights and simulation proxy unchanged. Visual and native cooked verification pending.'
), indent=2) + '\n')

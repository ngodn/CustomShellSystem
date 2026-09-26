"""Check retained collider skinning and excluded-face bounds against the new trajectory."""
import hashlib
import json
from pathlib import Path
import numpy as np

w = Path(__file__).resolve().parents[2]/'work/eve26'
output = w/'body-local-verified.json'
assert not output.exists()
baseline_path = w/'panel-fit-motion.json'
baseline = json.loads(baseline_path.read_text())
trial = json.loads((w/'panel-fit-local.json').read_text())
subset = json.loads((w/'body-local.json').read_text())
source = json.loads(Path(baseline['body_collision_input']).read_text())
assert hashlib.sha256(baseline_path.read_bytes()).hexdigest() == subset['motion_sha256']
assert hashlib.sha256(Path(baseline['body_collision_input']).read_bytes()).hexdigest() == subset['source_sha256']
ids = subset['source_vertex_indices']
retained = subset['source_triangle_indices']
for key in ('positions', 'weights', 'transfer'):
    assert subset[key] == [source[key][i] for i in ids]
assert [[ids[v] for v in f] for f in subset['indices']] == [source['indices'][i] for i in retained]
faces = np.asarray(source['indices'])
removed = np.ones(len(faces), dtype=bool)
removed[retained] = False
faces = faces[removed]
proxy = json.loads((w/'panel-fit-proxy.json').read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
rest = np.asarray(proxy['positions'])
alpha = np.clip((proxy['anchor_top_cm']-20-rest[:, 2])/12, 0, 1)
dynamic = 18*alpha**2*(3-2*alpha) >= .1
error = 0.
minimum = float('inf')
previous_body = previous_cloth = None
for original, current in zip(baseline['frames'], trial['frames'], strict=True):
    assert original['frame'] == current['frame']
    body = np.asarray(original['body_reference_skin_cm'])
    error = max(error, float(np.linalg.norm(body[ids]-current['body_reference_skin_cm'], axis=1).max()))
    cloth = np.asarray(current['positions_cm'])[dynamic]
    bmin, bmax = body[faces].min(axis=1), body[faces].max(axis=1)
    cmin, cmax = cloth.min(axis=0), cloth.max(axis=0)
    if previous_body is not None:
        bmin = np.minimum(bmin, previous_body[faces].min(axis=1))
        bmax = np.maximum(bmax, previous_body[faces].max(axis=1))
        cmin = np.minimum(cmin, previous_cloth.min(axis=0))
        cmax = np.maximum(cmax, previous_cloth.max(axis=0))
    minimum = min(minimum, float(np.linalg.norm(np.maximum(np.maximum(bmin-cmax, cmin-bmax), 0), axis=1).min()))
    previous_body, previous_cloth = body, cloth
assert error < .001
assert minimum > .45, 'Removed triangles approach the contact-search radius'
report = {'scope': 'This recorded sequence only. Uses native shape API skinning references and swept frame-endpoint bounds, not internal solver contact readback or unrecorded-motion safety.',
    'retained_native_body_max_error_cm': error, 'excluded_triangle_min_bound_distance_cm': minimum,
    'contact_search_radius_cm': .45, 'retained_geometry_and_weights_identical': True}
output.write_text(json.dumps(report, indent=2)+'\n')
print(report)

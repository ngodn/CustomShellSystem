"""Keep collider triangles near the swept cloth bounds of one measured sequence."""
import hashlib
import json
from pathlib import Path
import numpy as np

w = Path(__file__).resolve().parents[2]/'work/eve26'
output = w/'body-local.json'
assert not output.exists()
motion_path = w/'panel-fit-motion.json'
motion = json.loads(motion_path.read_text())
source_path = Path(motion['body_collision_input'])
body = json.loads(source_path.read_text())
proxy = json.loads((w/'panel-fit-proxy.json').read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
rest = np.asarray(proxy['positions'])
alpha = np.clip((proxy['anchor_top_cm']-20-rest[:, 2])/12, 0, 1)
dynamic = 18*alpha**2*(3-2*alpha) >= .1
faces = np.asarray(body['indices'])
keep = np.zeros(len(faces), dtype=bool)
closest = np.full(len(faces), np.inf)
previous_body = previous_cloth = None
margin = 5.
for frame in motion['frames']:
    points = np.asarray(frame['body_reference_skin_cm'])
    cloth = np.asarray(frame['positions_cm'])[dynamic]
    assert len(points) == len(body['positions']) and len(cloth) == frame['dynamic_particles']
    bmin, bmax = points[faces].min(axis=1), points[faces].max(axis=1)
    cmin, cmax = cloth.min(axis=0), cloth.max(axis=0)
    if previous_body is not None:
        bmin = np.minimum(bmin, previous_body[faces].min(axis=1))
        bmax = np.maximum(bmax, previous_body[faces].max(axis=1))
        cmin = np.minimum(cmin, previous_cloth.min(axis=0))
        cmax = np.maximum(cmax, previous_cloth.max(axis=0))
    distance = np.linalg.norm(np.maximum(np.maximum(bmin-cmax, cmin-bmax), 0), axis=1)
    closest = np.minimum(closest, distance)
    keep |= distance <= margin
    previous_body, previous_cloth = points, cloth
assert keep.any() and not keep.all()
used = np.unique(faces[keep])
lookup = {int(v): i for i, v in enumerate(used)}
result = {k: [body[k][i] for i in used] for k in ('positions', 'weights', 'transfer')}
result['indices'] = [[lookup[int(v)] for v in face] for face in faces[keep]]
result.update({'scope': 'Diagnostic collider subset for this recorded 69-frame sequence only. Unchanged retained vertices, weights and triangles. Five-centimetre swept-AABB margin excludes distant triangles; no guarantee for other clips, morphs or changed cloth trajectories. Not a production collision asset.',
    'source_vertex_indices': used.tolist(), 'source_triangle_indices': np.flatnonzero(keep).tolist(),
    'source_sha256': hashlib.sha256(source_path.read_bytes()).hexdigest(),
    'motion_sha256': hashlib.sha256(motion_path.read_bytes()).hexdigest(),
    'minimum_removed_triangle_bound_distance_cm': float(closest[~keep].min()), 'margin_cm': margin})
output.write_text(json.dumps(result)+'\n')
print(len(body['positions']), 'to', len(used), 'vertices;', len(faces), 'to', int(keep.sum()), 'triangles; excluded bound distance', closest[~keep].min())

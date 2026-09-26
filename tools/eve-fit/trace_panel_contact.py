"""Trace dynamic particles against native collider reference surfaces over recorded frames."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--motion', type=Path, required=True)
p.add_argument('--proxy', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
motion = json.loads(a.motion.read_text())
proxy = json.loads(a.proxy.read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
body_path = Path(motion['body_collision_input'])
body = json.loads(body_path.read_text())
rest = np.asarray(proxy['positions'])
alpha = np.clip((proxy['anchor_top_cm']-20-rest[:, 2])/12, 0, 1)
ids = np.flatnonzero(18*alpha**2*(3-2*alpha) >= .1)
faces = [[c, b, a] for a, b, c in body['indices']]
rows, crossings = [], []
previous = None
for frame in motion['frames']:
    tree = BVHTree.FromPolygons(frame['body_reference_skin_cm'], faces, all_triangles=True)
    distances = []
    for vertex in ids:
        point = Vector(frame['positions_cm'][vertex])
        near, normal, face, distance = tree.find_nearest(point)
        signed = (point-near).dot(normal)
        distances.append(signed)
    signed = np.asarray(distances)
    if previous is not None:
        for i in np.flatnonzero((previous >= -.1) & (signed < -.1)):
            crossings.append({'frame': frame['frame'], 'vertex': int(ids[i]),
                              'previous_signed_cm': float(previous[i]), 'signed_cm': float(signed[i])})
    worst = []
    for i in np.argsort(signed)[:8]:
        vertex = int(ids[i])
        if signed[i] >= -.1:
            continue
        rays = []
        for axis in ((1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)):
            origin = Vector(frame['positions_cm'][vertex])
            direction = Vector(axis)
            count = 0
            for _ in range(64):
                hit, _, _, _ = tree.ray_cast(origin, direction, 500)
                if hit is None:
                    break
                count += 1
                origin = hit+direction*.0001
            rays.append(count)
        worst.append({'vertex': vertex, 'signed_cm': float(signed[i]), 'ray_counts': rays})
    rows.append({'frame': frame['frame'], 'inside_over_1mm': int((signed < -.1).sum()),
                 'minimum_signed_cm': float(signed.min()), 'signed_cm': distances, 'worst': worst})
    previous = signed
report = {'scope': 'Frame-end particle samples against native collider skin-reference positions, not internal solver contacts or a continuous collision test. Nearest-normal signs can be ambiguous; worst samples include six ray counts. Substep trajectories are not observed.',
          'inputs': {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in (a.motion, a.proxy, body_path)},
          'dynamic_vertex_ids': ids.tolist(), 'frames': rows, 'crossings': crossings}
a.output.write_text(json.dumps(report, indent=2)+'\n')
print([(r['frame'], r['inside_over_1mm'], round(r['minimum_signed_cm'], 4)) for r in rows[-9:]])

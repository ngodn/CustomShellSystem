"""Locate rest-pose body undercoverage near the fitted garment."""
import hashlib
import json
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2] / 'work/eve26'
output = work / 'knit-coverage2.json'
assert not output.exists()
source = work / 'knit-export2/knit.mesh.json'
mesh = json.loads(source.read_text())
audit = json.loads(source.with_suffix('.audit.json').read_text())
recipe = json.loads((work / 'knit-spheres2.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest() == recipe['source_sha256'] == audit['output_sha256']
points = np.asarray(mesh['points'])
first = audit['parts'][0]['faces']
faces = [[mesh['wedges'][i][0] for i in face[:3]]
    for face in mesh['faces'][first:first+audit['parts'][1]['faces']]]
tree = BVHTree.FromPolygons(points.tolist(), faces, all_triangles=True)
ids = [i for i in range(audit['parts'][0]['points'])
    if tree.find_nearest(Vector(points[i]))[3] < 1]
centers = np.asarray([s['center_cm'] for s in recipe['spheres']])
radii = np.asarray([s['radius_cm'] for s in recipe['spheres']])
gaps = np.maximum(0, (np.linalg.norm(points[ids,None,:]-centers[None,:,:],axis=2)-radii).min(axis=1))
assert len(ids) == recipe['body_region_vertices']
assert abs(float(gaps.max())-recipe['body_surface_gap_cm']['maximum']) < 1e-6
result = dict(source_sha256=recipe['source_sha256'], vertices=ids,
    points_cm=points[ids].tolist(), gap_cm=gaps.tolist(),
    over_1cm=int((gaps > 1).sum()), over_2cm=int((gaps > 2).sum()),
    worst=[dict(vertex=ids[i], point_cm=points[ids[i]].tolist(), gap_cm=float(gaps[i]))
        for i in np.argsort(gaps)[-10:][::-1]],
    scope='Near-garment body vertices, rest pose only. Projection can overlap front and rear surfaces.')
output.write_text(json.dumps(result, separators=(',', ':'))+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('vertices','points_cm','gap_cm')}))

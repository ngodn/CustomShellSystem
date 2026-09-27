"""Measure sphere clearance against complete proxy triangles at rest."""
import hashlib
import json
from pathlib import Path

from mathutils import Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2] / 'work/eve26'
recipe_path = work / 'knit-spheres2.json'
proxy_path = work / 'knit-proxy1.json'
output = work / 'knit-spheres2-surface.json'
assert not output.exists()
recipe = json.loads(recipe_path.read_text())
proxy = json.loads(proxy_path.read_text())
assert recipe['source_sha256'] == proxy['report']['source_sha256']
slot = proxy['slots']['Collar-1']
indices = slot['indices']
assert len(indices) % 3 == 0
triangles = [indices[i:i+3] for i in range(0, len(indices), 3)]
tree = BVHTree.FromPolygons(slot['positions'], triangles, all_triangles=True)
rows = []
for index, sphere in enumerate(recipe['spheres']):
    point, _, triangle, distance = tree.find_nearest(Vector(sphere['center_cm']))
    assert point is not None
    rows.append(dict(sphere=index, bone=sphere['bone'], triangle=triangle,
        closest_point_cm=list(point), clearance_cm=distance-sphere['radius_cm']))
result = dict(
    inputs={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (recipe_path, proxy_path)},
    minimum_triangle_clearance_cm=min(row['clearance_cm'] for row in rows),
    spheres_intersecting_proxy=sum(row['clearance_cm'] < 0 for row in rows),
    spheres=rows,
    scope='Rest-pose triangle surface clearance only. Coverage gaps remain; no motion or cloth acceptance.')
output.write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k != 'spheres'}))

"""Build and measure a separate Knitwear simulation surface without changing render geometry."""
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

root = Path(__file__).resolve().parents[3]
work = root / 'CustomShellSystem/work/eve26'
sys.path.insert(0, str(root / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'))
from export_variant_clean import TO_UE
out = work / 'knit-proxy1.json'
assert not out.exists()
path = work / 'knit-export2/knit.mesh.json'
raw = path.read_bytes()
data = json.loads(raw)
audit = json.loads(path.with_suffix('.audit.json').read_text())
assert hashlib.sha256(raw).hexdigest() == audit['output_sha256']
maps = json.loads((work / 'knit-cloth-map.json').read_text())
assert maps['topology_identical']
start = audit['parts'][0]['points']
count = audit['parts'][1]['points']
assert audit['parts'][1]['name'] == 'Eve Extras - Sweater'
points = np.asarray(data['points'][start:start+count])
mapped = np.asarray([TO_UE @ Vector(p) for p in maps['points_m']])
assert np.max(np.linalg.norm(mapped-points, axis=1)) < .0005
first = audit['parts'][0]['faces']
faces = np.asarray([[data['wedges'][i][0]-start for i in f[:3]]
    for f in data['faces'][first:first+audit['parts'][1]['faces']]])
weights = [[] for _ in range(count)]
for v, b, weight in data['influences']:
    if start <= v < start+count:
        weights[v-start].append((data['bones'][b]['name'], weight))
def components(n, triangles):
    neighbors = [set() for _ in range(n)]
    for triangle in triangles:
        for v in triangle:
            neighbors[v].update(int(i) for i in triangle if i != v)
    remaining = set(range(n)); result = []
    while remaining:
        stack = [remaining.pop()]; size = 0
        while stack:
            v = stack.pop(); size += 1
            added = neighbors[v] & remaining
            remaining.difference_update(added); stack.extend(added)
        result.append(size)
    return sorted(result, reverse=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
mesh = bpy.data.meshes.new('KnitProxy')
mesh.from_pydata(points.tolist(), [], faces.tolist())
obj = bpy.data.objects.new('KnitProxy', mesh)
bpy.context.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
modifier = obj.modifiers.new('SimulationOnly', 'DECIMATE')
modifier.ratio = 3000 / len(faces)
modifier.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier=modifier.name)
obj.data.calc_loop_triangles()
tree = BVHTree.FromPolygons(points.tolist(), faces.tolist(), all_triangles=True)
positions, transferred, source_map, source_normals = [], [], [], []
render_faces = data['faces'][first:first+audit['parts'][1]['faces']]
attributes = {name:[] for name in maps['weights']}
for vertex in obj.data.vertices:
    hit, _, face, _ = tree.find_nearest(vertex.co)
    ids = faces[face]; origin, b, c = points[ids]
    uv = np.linalg.lstsq(np.column_stack((b-origin, c-origin)), np.asarray(hit)-origin, rcond=None)[0]
    bary = np.maximum([1-uv.sum(), *uv], 0); bary /= bary.sum()
    positions.append((bary @ points[ids]).tolist())
    normal = bary @ np.asarray([data['normals'][i] for i in render_faces[face][:3]])
    assert np.linalg.norm(normal) > .1
    source_normals.append(normal / np.linalg.norm(normal))
    combined = {}
    for i, factor in zip(ids, bary):
        for name, weight in weights[i]:
            combined[name] = combined.get(name, 0) + float(factor)*weight
    rows = sorted(((name, weight) for name, weight in combined.items() if weight > 0), key=lambda row:-row[1])[:8]
    total = sum(weight for _, weight in rows)
    assert total > 0
    transferred.append([[name, weight/total] for name, weight in rows])
    for name, values in maps['weights'].items():
        attributes[name].append(float(bary @ np.asarray(values)[ids]))
    source_map.append(dict(vertices=ids.tolist(), barycentric=bary.tolist()))
triangles = [list(t.vertices) for t in obj.data.loop_triangles]
positions = np.asarray(positions)
normals = np.asarray(source_normals)
for triangle in triangles:
    a, b, c = positions[triangle]
    normal = np.cross(b-a, c-a)
    assert np.linalg.norm(normal) > 1e-8, 'Degenerate proxy triangle'
assert np.allclose(np.linalg.norm(normals, axis=1), 1)
proxy_tree = BVHTree.FromPolygons(positions.tolist(), triangles, all_triangles=True)
distances = np.asarray([proxy_tree.find_nearest(Vector(v))[3] for v in points])
before, after = components(count, faces), components(len(positions), triangles)
report = dict(source_sha256=audit['output_sha256'], render_points=count,
    proxy_points=len(positions), proxy_triangles=len(triangles), source_components=before,
    proxy_components=after, render_to_proxy_cm=dict(maximum=float(distances.max()),
        p95=float(np.percentile(distances,95)), median=float(np.median(distances))),
    scope='Separate decimated surface with interpolated authored weights. No movement limits, native cloth binding, morph or motion acceptance.')
assert hashlib.sha256(path.read_bytes()).hexdigest() == audit['output_sha256']
out.write_text(json.dumps(dict(report=report, slots={'Collar-1':dict(positions=positions.tolist(),
    normals=normals.tolist(), indices=[i for t in triangles for i in t], weights=transferred,
    authored_maps=attributes, transfer=source_map)}), separators=(',', ':'))+'\n')
print(json.dumps(report), flush=True)

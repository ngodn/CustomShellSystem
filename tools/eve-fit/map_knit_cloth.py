"""Verify authored cloth-map topology against the fitted sweater library."""
import hashlib
import json
from pathlib import Path
import bpy

root = Path(__file__).resolve().parents[3]
work = root / 'CustomShellSystem/work/eve26'
original = root / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/reference/body-type-variant-EVE/eve_beta10.blend'
fitted = work / 'knit-f2.blend'
out = work / 'knit-cloth-map.json'
assert not out.exists()
hashes = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (original, fitted)}
name = 'Eve Extras - Sweater'
bpy.ops.wm.read_factory_settings(use_empty=True)
objects = []
for path in (original, fitted):
    with bpy.data.libraries.load(str(path), link=False) as (_, data):
        data.objects = [name]
    objects.append(data.objects[0])
author, candidate = objects
assert len(author.data.vertices) == len(candidate.data.vertices) == 19868
assert [list(p.vertices) for p in author.data.polygons] == [list(p.vertices) for p in candidate.data.polygons]
assert [list(e.vertices) for e in author.data.edges] == [list(e.vertices) for e in candidate.data.edges]
groups = {g.index:g.name for g in author.vertex_groups if g.name.startswith('dForce')}
weights = {name:[0.0]*len(author.data.vertices) for name in groups.values()}
for vertex in author.data.vertices:
    for group in vertex.groups:
        if group.group in groups:
            weights[groups[group.group]][vertex.index] = group.weight
points = [list(candidate.matrix_world @ v.co) for v in candidate.data.vertices]
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p,h in hashes.items())
out.write_text(json.dumps(dict(source_hashes=hashes, topology_identical=True,
    points_m=points, weights=weights,
    scope='Original vertex weights mapped by verified identical vertex/edge/polygon indices. No Unreal distance conversion or simulation acceptance.'), separators=(',', ':'))+'\n')
print('KNIT_CLOTH_MAP_VERIFIED', len(points))

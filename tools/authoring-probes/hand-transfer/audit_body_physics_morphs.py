"""Measure collider coverage at all 64 corners of the six public body morphs.

Analytic reference-pose skin coverage, not moving physics or damage acceptance.
"""
import hashlib
import itertools
import json
import os
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_BODY_MORPH_AUDIT_DIR']).resolve()
ASSET = Path(os.environ['CSS_BODY_MORPH_ASSET']).resolve()
assert OUT.parent == WORK.resolve() and not OUT.exists()
assert ASSET.is_relative_to(WORK.resolve())
OUT.mkdir()
sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools'))
from export_seduxtress_eve import read_bones
from body_physics_geometry import PUBLIC_MORPHS, skin_regions, sdf

source_path = WORK/'arm-rest-correctives-export-v1/candidate.mesh.json'
bind_path = WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
hashes = {str(p): digest(p) for p in (source_path, bind_path, ASSET)}
assert hashes[str(source_path)] == 'f49fd4e69a554f99bb2c309c78688192bd839285db178a9c83e013181b640d8a'
source = json.loads(source_path.read_text())
asset = json.loads(ASSET.read_text())
bones, world = read_bones(bind_path)
ids, regions = skin_regions(source, bones, asset['bodies'])
by_name = {b['name'].lower(): i for i, b in enumerate(bones)}
point_lookup = {vertex: index for index, vertex in enumerate(ids)}
vertex_bone = {vertex: name for name, vertices in regions.items() for vertex in vertices}
points = np.asarray(source['points'], dtype=float)[ids]
deltas = np.zeros((len(PUBLIC_MORPHS), len(ids), 3))
for shape in source['morph_targets']:
    if shape['name'] not in PUBLIC_MORPHS:
        continue
    index = PUBLIC_MORPHS.index(shape['name'])
    for vertex, *delta in shape['deltas']:
        if vertex in point_lookup:
            deltas[index, point_lookup[vertex]] = delta
local = []
for body in asset['bodies']:
    name = body['BoneName'].lower()
    inverse = np.asarray(world[by_name[name]].inverted(), dtype=float)
    local.append((body, inverse, np.asarray([point_lookup[v] for v in regions[name]])))
cases = []
worst = None
for corner, weights in enumerate(itertools.product((0., 1.), repeat=6)):
    cloud = points+np.einsum('i,ijk->jk', weights, deltas)
    union = np.full(len(ids), np.inf)
    assigned = []
    for body, inverse, region in local:
        value = cloud@inverse[:3, :3].T+inverse[:3, 3]
        distance = np.min(np.stack([sdf(shape, kind, value)
            for kind, shapes in body['AggGeom'].items() for shape in shapes]), axis=0)
        union = np.minimum(union, distance)
        assigned.append(dict(bone=body['BoneName'], outside=int(np.sum(distance[region] > .001)),
            maximum_outside_cm=max(0., float(distance[region].max()))))
    index = int(np.argmax(union))
    row = dict(corner=corner, weights=list(weights), outside=int(np.sum(union > .001)),
        maximum_outside_cm=max(0., float(union[index])), worst_vertex=ids[index], worst_bone=vertex_bone[ids[index]], worst_point=list(cloud[index]), assigned=assigned)
    cases.append(row)
    if worst is None or row['maximum_outside_cm'] > worst['maximum_outside_cm']:
        worst = dict(row)
assert all(digest(Path(p)) == h for p, h in hashes.items())
(OUT/'report.json').write_text(json.dumps(dict(scope=__doc__, asset=str(ASSET), morphs=PUBLIC_MORPHS,
    protected_hashes=hashes, skin_vertices=len(ids), cases=cases, worst=worst,
    all_corners_enclosed=all(row['outside'] == 0 for row in cases)), indent=2)+'\n')
print(json.dumps(dict(corners=len(cases), failing_corners=sum(c['outside'] > 0 for c in cases),
    maximum_outside_cm=worst['maximum_outside_cm'], worst_corner=worst['corner'])), flush=True)

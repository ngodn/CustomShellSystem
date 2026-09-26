"""Identify surfaces in the small viewer-left Holiday hip patch using front-facing rays."""
import json
import argparse
import sys
from pathlib import Path

from mathutils import Vector
from mathutils.bvhtree import BVHTree

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesh', type=Path, default=WORK/'holiday.mesh.json')
parser.add_argument('--output', type=Path, default=WORK/'hip-patch-nearest.json')
parser.add_argument('--all-morphs', action='store_true')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
data = json.loads(args.mesh.read_text())
faces = [[data['wedges'][w][0] for w in f[:3]] for f in data['faces']]
direc = Vector((0, -1, 0))
reports = []
cases = [('default', []), ('hip-waist', ['PBMHipSize', 'PBMWaistWidth'])]
if args.all_morphs:
    body_shapes = [m['name'] for m in data['morph_targets'] if not m['name'].startswith('pJCM')]
    cases += [(name, [name]) for name in body_shapes] + [('all-body', body_shapes)]
for label, names in cases:
    points = [Vector(p) for p in data['points']]
    for morph in data['morph_targets']:
        if morph['name'] in names:
            for i, *delta in morph['deltas']:
                points[i] += Vector(delta)
    tree = BVHTree.FromPolygons(points, faces, all_triangles=True)
    dress_faces = [f for i, f in enumerate(faces) if data['materials'][data['faces'][i][3]] == 'MI_CH_P_EVE_Christmas_01_01.001']
    dress_tree = BVHTree.FromPolygons(points, dress_faces, all_triangles=True)
    hits, visible = [], {}
    for ix in range(49):
        for iz in range(65):
            x, z = -24+ix*.25, 102+iz*.25
            hit, normal, index, distance = tree.ray_cast(Vector((x, 100, z)), direc, 200)
            if index is None:
                continue
            material = data['materials'][data['faces'][index][3]]
            visible.setdefault(material, []).append(list(hit))
            if material != 'MI_CH_P_EVE_Christmas_01_01.005':
                continue
            next_hit, _, next_index, depth = tree.ray_cast(hit+direc*.0001, direc, 2)
            if next_index is not None and 16 <= data['faces'][next_index][3] <= 20:
                nearest, _, nearest_index, separation = dress_tree.find_nearest(hit)
                hits.append({'position_cm': list(hit), 'visible_material': data['materials'][data['faces'][index][3]],
                             'behind_material': data['materials'][data['faces'][next_index][3]],
                             'depth_cm': depth+.0001, 'cloth_face': next_index,
                             'nearest_cloth_cm': list(nearest), 'nearest_separation_cm': separation,
                             'nearest_cloth_vertices': dress_faces[nearest_index]})
    reports.append({'case': label, 'samples': len(hits), 'hits': hits,
                    'visible': {k: {'samples': len(v), 'bounds_cm': [[min(p[i] for p in v), max(p[i] for p in v)] for i in range(3)]} for k, v in visible.items()}})
path = args.output
assert not path.exists(), path
path.write_text(json.dumps({'cases': reports, 'scope': 'Front rays in x=-24..-12, z=102..118 cm for listed morph cases at weight 1. Not full-surface collision validation.'}, indent=2)+'\n')
for r in reports:
    print(r['case'], r['samples'], sorted({(h['visible_material'], h['behind_material']) for h in r['hits']}))
    if r['hits']:
        print('max depth cm', max(h['depth_cm'] for h in r['hits']))

"""Add private rest-fitted sphere probes following the body's existing secondary bones."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--recipe', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
mesh = json.loads((w/'holiday-waist-source.mesh.json').read_text())
body = json.loads((w/'body-collider.json').read_text())
recipe = json.loads(a.recipe.read_text())
bind = []
by_name = {}
for bone in mesh['bones']:
    q = bone['rotation']
    m = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@m if bone['parent'] >= 0 else m)
    by_name[bone['name']] = bind[-1]
tree = BVHTree.FromPolygons(body['positions'], [[c, b, a] for a, b, c in body['indices']], all_triangles=True)
original_count = len(recipe['spheres'])
for bone, x in (('butt001', 7.), ('butt002', -7.)):
    assert bone in by_name
    for z in (103., 110.):
        center = Vector((x, -6., z))
        votes = []
        for axis in ((1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)):
            origin, direction, count = center.copy(), Vector(axis), 0
            for _ in range(64):
                hit, _, _, _ = tree.ray_cast(origin, direction, 500)
                if hit is None:
                    break
                count += 1
                origin = hit+direction*.0001
            else:
                raise ValueError('Ray traversal limit')
            votes.append(count % 2 == 1)
        assert all(votes), (bone, z, votes)
        _, _, _, distance = tree.find_nearest(center)
        assert distance > .65
        recipe['spheres'].append({'bone': bone, 'center_cm': list(center),
                                  'local_center_cm': list(by_name[bone].inverted()@center),
                                  'radius_cm': distance-.15})
recipe['secondary_probe'] = {'source': str(a.recipe),
                             'source_sha256': hashlib.sha256(a.recipe.read_bytes()).hexdigest(),
                             'original_spheres': original_count, 'added_spheres': len(recipe['spheres'])-original_count}
recipe['scope'] = 'Four additional rest-inscribed spheres on existing secondary bones. Earlier source coverage statistics are historical, not measurements of this candidate. Requires posed/morph and native checks; no production changes.'
for key in ('body_surface_gap_cm', 'worst_body_points'):
    recipe.pop(key, None)
a.output.write_text(json.dumps(recipe, indent=2)+'\n')
print(recipe['spheres'][original_count:])

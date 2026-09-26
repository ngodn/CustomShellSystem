"""Fit an isolated collision trial to the preserved exported body, in centimetres."""
import argparse
import sys
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rear',action='store_true')
a=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
data = json.loads((WORK/'planet-export/planet.mesh.json').read_text())
audit = json.loads((WORK/'planet-export/planet.mesh.audit.json').read_text())
assert audit['parts'][0]['name'] == 'Eve Body'
faces = [[data['wedges'][w][0] for w in f[:3]] for f in data['faces'][:audit['parts'][0]['faces']]]
points = [Vector(p) for p in data['points']]
tree = BVHTree.FromPolygons(points,faces,all_triangles=True)
world = []
for bone in data['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    world.append(world[bone['parent']]@local if bone['parent']>=0 else local)
bind = {b['name']:world[i] for i,b in enumerate(data['bones'])}
def inside(center):
    votes = []
    for direction in (Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1))):
        origin = center.copy()
        hits = 0
        for _ in range(100):
            hit,_,_,_ = tree.ray_cast(origin,direction,300)
            if hit is None:
                break
            hits += 1
            origin = hit+direction*.0001
        else:
            raise ValueError('Ray limit')
        votes.append(hits%2 == 1)
    return votes
seeds = [('pelvis',Vector((x,-3,z))) for z in (105,111,117) for x in (-7,0,7)]
for side in ('l','r'):
    name = 'thigh_'+side
    start = bind[name].translation
    calf = bind['calf_'+side].translation
    for fraction in (.15,.4,.65,.85):
        seeds.append((name,start.lerp(calf,fraction)))
for side in ('l','r'):
    name='calf_'+side
    for fraction in (.15,.45,.75):
        seeds.append((name,bind[name].translation.lerp(bind['foot_'+side].translation,fraction)))
if a.rear:
    seeds.extend(('pelvis',Vector((x,-10,z))) for x in (-5,5) for z in (100,104,108))
spheres, rejected = [],[]
for bone,center in seeds:
    votes = inside(center)
    if not all(votes):
        rejected.append({'bone':bone,'center':list(center),'inside_votes':votes})
        continue
    _,_,_,distance = tree.find_nearest(center)
    radius = distance-.15
    assert radius>0
    spheres.append({'bone':bone,'center_cm':list(center),'local_center_cm':list(bind[bone].inverted()@center),'radius_cm':radius})
proxy = np.asarray(json.loads((WORK/'planet-tail-cloth.json').read_text())['slots']['PlanetTail_17']['positions'])
def gap(vertices):
    return np.min(np.stack([np.linalg.norm(vertices-np.asarray(s['center_cm']),axis=1)-s['radius_cm'] for s in spheres]),axis=0)
clearance = gap(proxy)
body_ids = sorted({v for f in faces for v in f})
region = np.asarray([data['points'][i] for i in body_ids if 40 <= data['points'][i][2] <= 118 and abs(data['points'][i][0])<12 and data['points'][i][1]<0])
coverage = gap(region)
initial_coverage = float(np.quantile(coverage,.95))
clearance = gap(proxy)
out = WORK/('planet-tail-rear-spheres.json' if a.rear else 'planet-tail-spheres.json')
assert not out.exists()
report = {'spheres':spheres,'connections':[],'sphere_count':len(spheres),'initial_p95_gap_cm':initial_coverage,'rejected_seeds':rejected,'tail_inside_vertices':int((clearance<0).sum()),
          'minimum_tail_clearance_cm':float(clearance.min()),'body_region_vertices':len(region),
          'body_surface_gap_cm':{'median':float(np.median(coverage)),'p95':float(np.quantile(coverage,.95)),'max':float(coverage.max())},
          'scope':'Inscribed rest-pose spheres only; gaps quantify incomplete body coverage; connections, motion and morph validation pending'}
assert report['tail_inside_vertices']==0, 'Candidate intersects the authored tail proxy'
out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='spheres'},indent=2))

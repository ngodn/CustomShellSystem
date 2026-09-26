"""Fit an isolated collision trial to the preserved exported body, in centimetres."""
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
data = json.loads((WORK/'holiday.mesh.json').read_text())
audit = json.loads((WORK/'holiday.mesh.audit.json').read_text())
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
proxy = np.asarray(json.loads((WORK/'skirt-surface.json').read_text())['positions'])
def gap(vertices):
    return np.min(np.stack([np.linalg.norm(vertices-np.asarray(s['center_cm']),axis=1)-s['radius_cm'] for s in spheres]),axis=0)
clearance = gap(proxy)
body_ids = sorted({v for f in faces for v in f})
region = np.asarray([data['points'][i] for i in body_ids if 92 <= data['points'][i][2] <= 118 and abs(data['points'][i][0])<22])
coverage = gap(region)
initial_coverage = float(np.quantile(coverage,.95))
# Fill the largest uncovered surface regions with additional inscribed spheres.
# This is a bounded trial, not permission to grow runtime collision indefinitely.
while len(spheres)<32 and np.quantile(coverage,.95)>1:
    best = None
    for index in np.argsort(coverage)[-12:]:
        surface, normal, _, _ = tree.find_nearest(Vector(region[index]))
        for depth in (2.,4.,6.,8.):
            center = surface+normal*depth
            if not all(inside(center)):
                continue
            _,_,_,distance = tree.find_nearest(center)
            radius = distance-.15
            if radius <= .5:
                continue
            distances = np.linalg.norm(region-np.asarray(center),axis=1)-radius
            proposed = np.minimum(coverage,distances)
            improvement = float(np.maximum(coverage-proposed,0).sum())
            if best is None or improvement>best[0]:
                best = (improvement,center,radius,proposed)
    if best is None or best[0]<.01:
        break
    _,center,radius,coverage = best
    bone = 'pelvis' if center.z>105 else ('thigh_l' if center.x>0 else 'thigh_r')
    spheres.append({'bone':bone,'center_cm':list(center),'local_center_cm':list(bind[bone].inverted()@center),'radius_cm':radius})
clearance = gap(proxy)
out = WORK/'cloth-spheres2.json'
assert not out.exists()
report = {'spheres':spheres,'sphere_count':len(spheres),'initial_p95_gap_cm':initial_coverage,'rejected_seeds':rejected,'skirt_inside_vertices':int((clearance<0).sum()),
          'minimum_skirt_clearance_cm':float(clearance.min()),'body_region_vertices':len(region),
          'body_surface_gap_cm':{'median':float(np.median(coverage)),'p95':float(np.quantile(coverage,.95)),'max':float(coverage.max())},
          'scope':'Inscribed rest-pose spheres only; gaps quantify incomplete body coverage; connections, motion and morph validation pending'}
out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='spheres'},indent=2))

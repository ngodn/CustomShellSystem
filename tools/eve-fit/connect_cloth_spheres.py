"""Evaluate same-bone tapered connections between fitted cloth collision spheres."""
import json
from itertools import combinations
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
candidate = json.loads((WORK/'cloth-spheres2.json').read_text())
data = json.loads((WORK/'holiday.mesh.json').read_text())
audit = json.loads((WORK/'holiday.mesh.audit.json').read_text())
faces = [[data['wedges'][w][0] for w in f[:3]] for f in data['faces'][:audit['parts'][0]['faces']]]
tree = BVHTree.FromPolygons([Vector(p) for p in data['points']],faces,all_triangles=True)
ids = sorted({v for f in faces for v in f})
region = np.asarray([data['points'][i] for i in ids if 92 <= data['points'][i][2] <= 118 and abs(data['points'][i][0])<22])
proxy = np.asarray(json.loads((WORK/'skirt-surface.json').read_text())['positions'])
spheres = candidate['spheres']
coverage = np.min(np.stack([np.linalg.norm(region-np.asarray(s['center_cm']),axis=1)-s['radius_cm'] for s in spheres]),axis=0)
def gap(points, a, b, r0, r1):
    delta = b-a
    length = np.linalg.norm(delta)
    dr = r1-r0
    axis = delta/length
    along = (points-a)@axis
    perpendicular = np.linalg.norm(points-a-along[:,None]*axis,axis=1)
    t = np.clip((along+dr*perpendicular/np.sqrt(length*length-dr*dr))/length,0,1)
    return np.linalg.norm(points-(a+t[:,None]*delta),axis=1)-(r0+t*dr)
options = []
for i,j in combinations(range(len(spheres)),2):
    sa,sb = spheres[i],spheres[j]
    if sa['bone']!=sb['bone']:
        continue
    a,b = np.asarray(sa['center_cm']),np.asarray(sb['center_cm'])
    r0,r1 = sa['radius_cm'],sb['radius_cm']
    length = np.linalg.norm(b-a)
    if not 2<length<16 or abs(r1-r0)>=length:
        continue
    # Reject connections protruding beyond the nearest body surface along the axis.
    excess = []
    for t in np.linspace(0,1,17):
        _,_,_,distance = tree.find_nearest(Vector(a+t*(b-a)))
        excess.append(r0+t*(r1-r0)-distance)
    if max(excess)>.1:
        continue
    skirt_gap = gap(proxy,a,b,r0,r1)
    if skirt_gap.min()<.2:
        continue
    options.append((i,j,gap(region,a,b,r0,r1),float(skirt_gap.min()),max(excess)))
chosen = []
for _ in range(16):
    if not options:
        break
    scores = [float(np.maximum(coverage-o[2],0).sum()) for o in options]
    best = int(np.argmax(scores))
    if scores[best]<.1:
        break
    i,j,distances,clearance,excess = options.pop(best)
    coverage = np.minimum(coverage,distances)
    chosen.append({'sphere_indices':[i,j],'bone':spheres[i]['bone'],'minimum_skirt_clearance_cm':clearance,'sampled_axis_radius_excess_cm':excess})
report = {'spheres':spheres,'connections':chosen,'eligible_connections':len(options)+len(chosen),
          'body_surface_gap_cm':{'median':float(np.median(coverage)),'p95':float(np.quantile(coverage,.95)),'max':float(coverage.max())},
          'worst_body_points':[{'position_cm':region[i].tolist(),'gap_cm':float(coverage[i])} for i in np.argsort(coverage)[-12:][::-1]],
          'scope':'Rest-pose tapered-sphere envelope trial; sampled axis containment, no animated coverage or native validation'}
output = WORK/'cloth-capsules.json'
assert not output.exists()
output.write_text(json.dumps(report,indent=2)+'\n')
print('Connections:',len(chosen),'remaining:',len(options))
print(report['body_surface_gap_cm'])

"""Select up to 32 inscribed surface-seeded spheres and report rest-pose coverage."""
import hashlib
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

w = Path(__file__).resolve().parents[2] / 'work/eve26'
out = w / 'knit-spheres2.json'
assert not out.exists()
path = w / 'knit-export2/knit.mesh.json'
data = json.loads(path.read_text())
audit = json.loads(path.with_suffix('.audit.json').read_text())
assert hashlib.sha256(path.read_bytes()).hexdigest() == audit['output_sha256']
nb = audit['parts'][0]['points']; nf = audit['parts'][0]['faces']
points = np.asarray(data['points'])
faces = np.asarray([[data['wedges'][i][0] for i in f[:3]] for f in data['faces'][:nf]])
body = BVHTree.FromPolygons(points[:nb].tolist(), faces.tolist(), all_triangles=True)
gf = [[data['wedges'][i][0] for i in f[:3]] for f in data['faces'][nf:nf+audit['parts'][1]['faces']]]
garment = BVHTree.FromPolygons(points.tolist(), gf, all_triangles=True)
ids = [i for i in sorted(set(faces.flatten())) if garment.find_nearest(Vector(points[i]))[3] < 1]
region = points[ids]
proxy = np.asarray(json.loads((w/'knit-proxy1.json').read_text())['slots']['Collar-1']['positions'])
normals = np.zeros((nb,3))
for triangle in faces:
    a,b,c = points[triangle]
    normals[triangle] += np.cross(b-a,c-a)
weights = {}
for v,b,value in data['influences']:
    if v < nb and (v not in weights or value > weights[v][1]):
        weights[v] = (b,value)
world = []
for bone in data['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    world.append(world[bone['parent']] @ local if bone['parent'] >= 0 else local)
def inside(center):
    for direction in (Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1))):
        origin = Vector(center); hits = 0
        for _ in range(100):
            hit,_,_,_ = body.ray_cast(origin,direction,300)
            if hit is None: break
            hits += 1; origin = hit + direction*.0001
        else: raise ValueError('Ray limit')
        if hits % 2 != 1: return False
    return True
candidates = []
for i in ids:
    length = np.linalg.norm(normals[i])
    if length < 1e-10: continue
    normal = normals[i] / length
    for depth in (3.,5.,8.):
        for sign in (-1,1):
            center = points[i] + sign*depth*normal
            if not inside(center): continue
            radius = min(body.find_nearest(Vector(center))[3]-.15,
                float(np.linalg.norm(proxy-center,axis=1).min())-.15)
            if radius < .5: continue
            bone = weights[i][0]
            candidates.append(dict(bone=data['bones'][bone]['name'], center_cm=center.tolist(),
                local_center_cm=list(world[bone].inverted() @ Vector(center)),radius_cm=radius,
                source_vertex=int(i)))
assert candidates
centers = np.asarray([s['center_cm'] for s in candidates])
radii = np.asarray([s['radius_cm'] for s in candidates])
gaps = np.linalg.norm(region[None,:,:]-centers[:,None,:],axis=2)-radii[:,None]
current = np.full(len(region),30.); chosen = []
for _ in range(min(32,len(candidates))):
    gains = np.maximum(current[None,:]-np.maximum(gaps,0),0).sum(axis=1)
    gains[chosen] = -1
    selected = int(np.argmax(gains))
    if gains[selected] < 1e-6: break
    chosen.append(selected); current = np.minimum(current,np.maximum(gaps[selected],0))
spheres = [candidates[i] for i in chosen]
clearance = np.min(np.linalg.norm(proxy[None,:,:]-centers[chosen,None,:],axis=2)-radii[chosen,None],axis=0)
assert clearance.min() >= .14999
report = dict(source_sha256=audit['output_sha256'], spheres=spheres, connections=[],
    candidate_count=len(candidates), sphere_count=len(spheres), body_region_vertices=len(region),
    minimum_proxy_clearance_cm=float(clearance.min()), proxy_inside_vertices=int((clearance<0).sum()),
    body_surface_gap_cm=dict(median=float(np.median(current)),p95=float(np.percentile(current,95)),maximum=float(current.max())),
    scope='Greedy rest-pose inscribed sphere coverage. No animation, morph, native collision or cloth acceptance.')
out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='spheres'}),flush=True)

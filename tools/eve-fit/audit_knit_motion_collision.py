"""Find impossible cloth-distance/collider combinations in recorded Knitwear poses."""
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2]/'work/eve26'
out = work/'knit-w2/collision-body.json'
assert not out.exists()
mesh = json.loads((work/'knit-w2/knit.mesh.json').read_text())
proxy = json.loads((work/'knit-cloth2/proxy.json').read_text())['slots']['Collar-1']
recipe = json.loads((work/'knit-spheres2.json').read_text())
motion = json.loads((work/'knit-cloth1/sprint.json').read_text())
lookup = {b['name']:i for i,b in enumerate(mesh['bones'])}
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    bind.append(bind[bone['parent']] @ local if bone['parent'] >= 0 else local)
rows = np.asarray([[i,lookup[b],w] for i,weights in enumerate(proxy['weights']) for b,w in weights])
vi,bi,wt = rows[:,0].astype(int),rows[:,1].astype(int),rows[:,2]
points = np.asarray(proxy['positions'])
limits = np.asarray(proxy['max_distances'])
cases = []
worst_body = None
for frame in motion['frames']:
    snap = frame['pose']['Snapshot']
    entries = dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True))
    pose = []
    for bone in mesh['bones']:
        t = entries[bone['name']]
        local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[bone['parent']] @ local if bone['parent'] >= 0 else local)
    matrices = np.asarray([np.asarray(a @ b.inverted()) for a,b in zip(pose,bind,strict=True)])
    skinned = np.zeros_like(points)
    np.add.at(skinned,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],points[vi])+matrices[bi,:3,3])*wt[:,None])
    impossible = np.zeros(len(points),dtype=bool)
    worst = dict(deficit_cm=0.)
    for sphere in recipe['spheres']:
        transform = pose[lookup[sphere['bone']]]
        assert max(abs(v-1) for v in transform.to_scale()) < .001
        center = np.asarray(transform @ Vector(sphere['local_center_cm']))
        penetration = sphere['radius_cm']+.15-np.linalg.norm(skinned-center,axis=1)
        deficit = penetration-limits
        impossible |= (limits>0) & (deficit>1e-4)
        movable = np.where(limits>0,deficit,-np.inf)
        vertex = int(np.argmax(movable))
        if movable[vertex] > worst['deficit_cm']:
            worst = dict(deficit_cm=float(movable[vertex]),vertex=vertex,bone=sphere['bone'],
                penetration_cm=float(penetration[vertex]),limit_cm=float(limits[vertex]))
    cases.append(dict(frame=frame['frame'],impossible_vertices=int(impossible.sum()),worst=worst))
    if worst['deficit_cm'] > (worst_body['deficit_cm'] if worst_body else 0):
        audit = json.loads((work/'knit-w2/knit.mesh.audit.json').read_text())
        count = audit['parts'][0]['points']
        body_rows = np.asarray([row for row in mesh['influences'] if row[0] < count])
        bv,bb,bw = body_rows[:,0].astype(int),body_rows[:,1].astype(int),body_rows[:,2]
        rest = np.asarray(mesh['points'])[:count]
        body = np.zeros_like(rest)
        np.add.at(body,bv,(np.einsum('nij,nj->ni',matrices[bb,:3,:3],rest[bv])+matrices[bb,:3,3])*bw[:,None])
        faces = [[mesh['wedges'][i][0] for i in f[:3]] for f in mesh['faces'][:audit['parts'][0]['faces']]]
        tree = BVHTree.FromPolygons(body.tolist(),faces,all_triangles=True)
        point = Vector(skinned[worst['vertex']])
        hit,normal,face,distance = tree.find_nearest(point)
        votes = []
        for direction in (Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1))):
            origin = point.copy()
            hits = 0
            for _ in range(100):
                found,_,_,_ = tree.ray_cast(origin,direction,400)
                if found is None: break
                hits += 1
                origin = found+direction*.0001
            else: raise AssertionError('Body ray limit')
            votes.append(hits % 2)
        worst_body = dict(frame=frame['frame'],deficit_cm=worst['deficit_cm'],
            point_cm=list(point),nearest_body_cm=list(hit),body_distance_cm=distance,
            body_containment='inside' if all(votes) else 'outside' if not any(votes) else 'ambiguous')
report = dict(cases=cases,worst=max(cases,key=lambda c:c['worst']['deficit_cm']),
    worst_body_comparison=worst_body,
    collision_thickness_cm=.15,
    scope='Analytic recorded-pose sphere test using the authored recipe. Positive deficit means no point inside the max-distance ball can escape that sphere. Excludes solver interpolation, other constraints and rendered body coverage; not native collider readback.')
out.write_text(json.dumps(report,indent=2)+'\n')
print(report['worst'])

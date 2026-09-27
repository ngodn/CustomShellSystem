"""Compare existing collider parent choices against recorded body deformation."""
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2]/'work/eve26'
output = work/'knit-w2/collider-parents.json'
assert not output.exists()
mesh = json.loads((work/'knit-w2/knit.mesh.json').read_text())
audit = json.loads((work/'knit-w2/knit.mesh.audit.json').read_text())
recipe = json.loads((work/'knit-spheres2.json').read_text())
motion = json.loads((work/'knit-cloth2/sprint.json').read_text())
count = audit['parts'][0]['points']
base = np.asarray(mesh['points'])[:count]
faces = [[mesh['wedges'][i][0] for i in f[:3]] for f in mesh['faces'][:audit['parts'][0]['faces']]]
rows = np.asarray([r for r in mesh['influences'] if r[0] < count])
vi,bi,wt = rows[:,0].astype(int),rows[:,1].astype(int),rows[:,2]
lookup = {b['name']:i for i,b in enumerate(mesh['bones'])}
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    bind.append(bind[bone['parent']] @ local if bone['parent'] >= 0 else local)
candidates = []
for index,sphere in enumerate(recipe['spheres']):
    names = {sphere['bone']}
    names.update(mesh['bones'][int(b)]['name'] for v,b,w in rows if int(v)==sphere['source_vertex'] and w>.01)
    for name in sorted(names):
        candidates.append(dict(sphere=index,bone=name,original=name==sphere['bone'],
            local_center=list(bind[lookup[name]].inverted() @ Vector(sphere['center_cm'])),samples=[]))
def inside(tree,point):
    votes=[]
    for direction in (Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1))):
        origin=Vector(point);hits=0
        for _ in range(100):
            hit,_,_,_=tree.ray_cast(origin,direction,400)
            if hit is None: break
            hits+=1;origin=hit+direction*.0001
        else: raise AssertionError('Ray limit')
        votes.append(hits%2)
    return 'inside' if all(votes) else 'outside' if not any(votes) else 'ambiguous'
for frame in (0,8,16,24,32,40,48,56,64):
    snap=motion['frames'][frame]['pose']['Snapshot']
    entries=dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True));pose=[]
    for bone in mesh['bones']:
        t=entries[bone['name']]
        local=Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[bone['parent']] @ local if bone['parent']>=0 else local)
    matrices=np.asarray([np.asarray(a @ b.inverted()) for a,b in zip(pose,bind,strict=True)])
    body=np.zeros_like(base)
    np.add.at(body,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],base[vi])+matrices[bi,:3,3])*wt[:,None])
    tree=BVHTree.FromPolygons(body.tolist(),faces,all_triangles=True)
    for candidate in candidates:
        sphere=recipe['spheres'][candidate['sphere']]
        center=pose[lookup[candidate['bone']]] @ Vector(candidate['local_center'])
        _,_,_,distance=tree.find_nearest(center)
        containment=inside(tree,center)
        candidate['samples'].append(dict(frame=frame,center=list(center),containment=containment,
            nearest_surface_cm=distance,protrusion_cm=sphere['radius_cm']-(distance if containment=='inside' else -distance)))
for candidate in candidates:
    candidate['worst_protrusion_cm']=max(r['protrusion_cm'] for r in candidate['samples'])
    candidate['outside_or_ambiguous']=sum(r['containment']!='inside' for r in candidate['samples'])
output.write_text(json.dumps(dict(candidates=candidates,
    scope='Nine default-morph poses, fixed rest centers/radii. Protrusion uses nearest posed body surface and three-ray center containment. Parent comparison only; no native replacement, garment clearance or union coverage acceptance.'),indent=2)+'\n')
for index in range(len(recipe['spheres'])):
    group=[c for c in candidates if c['sphere']==index]
    original=next(c for c in group if c['original'])
    best=min(group,key=lambda c:(c['outside_or_ambiguous'],c['worst_protrusion_cm']))
    if original['worst_protrusion_cm']>.5:
        print(index,original['bone'],round(original['worst_protrusion_cm'],3),'best',best['bone'],round(best['worst_protrusion_cm'],3),flush=True)

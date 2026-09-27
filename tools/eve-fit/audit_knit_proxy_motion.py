"""Check proxy/body contact direction in sampled poses before cloth simulation."""
import hashlib
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2] / 'work/eve26'
output = work / 'knit-proxy-motion2.json'
assert not output.exists()
mesh_path = work / 'knit-export2/knit.mesh.json'
proxy_path = work / 'knit-proxy2.json'
motion_path = work / 'feet-sprint-base.json'
mesh = json.loads(mesh_path.read_text())
audit = json.loads(mesh_path.with_suffix('.audit.json').read_text())
proxy = json.loads(proxy_path.read_text())
assert hashlib.sha256(mesh_path.read_bytes()).hexdigest() == audit['output_sha256'] == proxy['report']['source_sha256']
slot = proxy['slots']['Collar-1']
nb = audit['parts'][0]['points']
body_faces = [[mesh['wedges'][i][0] for i in f[:3]] for f in mesh['faces'][:audit['parts'][0]['faces']]]
base = np.asarray(mesh['points'])
morph = np.zeros_like(base)
names = {'FBMBodyTone','PBMBreastsSize','PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth'}
for target in mesh['morph_targets']:
    if target['name'] in names:
        for i, *delta in target['deltas']:
            morph[i] += delta
proxy_morph = np.asarray([np.asarray(row['barycentric']) @ morph[np.asarray(row['vertices'])+nb]
    for row in slot['transfer']])
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3],*q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']] @ local if bone['parent'] >= 0 else local)
lookup = {bone['name']: i for i, bone in enumerate(mesh['bones'])}
body_weights = np.asarray([row for row in mesh['influences'] if row[0] < nb])
ng = audit['parts'][1]['points']
garment_weights = np.asarray([[v-nb,b,w] for v,b,w in mesh['influences'] if nb <= v < nb+ng])
transfer_ids = np.asarray([r['vertices'] for r in slot['transfer']])
transfer_bary = np.asarray([r['barycentric'] for r in slot['transfer']])
proxy_weights = np.asarray([[i,lookup[name],weight] for i,rows in enumerate(slot['weights']) for name,weight in rows])
def blend(matrices, weights, count):
    result = np.zeros((count,4,4))
    np.add.at(result, weights[:,0].astype(int), matrices[weights[:,1].astype(int)]*weights[:,2,None,None])
    return result
def skin(points, matrices):
    return np.einsum('nij,nj->ni',matrices[:,:3,:3],points)+matrices[:,:3,3]
def containment(tree, point):
    votes = []
    for direction in (Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1))):
        start = Vector(point); hits = 0
        for _ in range(100):
            hit,_,_,_ = tree.ray_cast(start,direction,400)
            if hit is None: break
            hits += 1; start = hit+direction*.0001
        else: raise ValueError('Ray limit')
        votes.append(hits % 2)
    return 'inside' if all(votes) else 'outside' if not any(votes) else 'ambiguous'
motion = json.loads(motion_path.read_text())
cases = []
for frame in (-1,24,32):
    pose = bind
    if frame >= 0:
        snapshot = motion['frames'][frame]['upstream']
        entries = dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
        pose = []
        for bone in mesh['bones']:
            t = entries[bone['name']]
            local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),
                Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
            pose.append(pose[bone['parent']] @ local if bone['parent'] >= 0 else local)
    matrices = np.asarray([np.asarray(a @ b.inverted()) for a,b in zip(pose,bind)])
    body_matrices = blend(matrices,body_weights,nb)
    garment_matrices = blend(matrices,garment_weights,ng)
    proxy_matrices = blend(matrices,proxy_weights,len(slot['positions']))
    normals = np.einsum('nij,nj->ni',proxy_matrices[:,:3,:3],slot['normals'])
    normals /= np.linalg.norm(normals,axis=1)[:,None]
    for maximum in (False,True):
        body = skin(base[:nb]+(morph[:nb] if maximum else 0),body_matrices)
        points = skin(np.asarray(slot['positions'])+(proxy_morph if maximum else 0),proxy_matrices)
        garment = skin(base[nb:nb+ng]+(morph[nb:nb+ng] if maximum else 0),garment_matrices)
        reference = np.einsum('ni,nij->nj',transfer_bary,garment[transfer_ids])
        error = np.linalg.norm(points-reference,axis=1)
        tree = BVHTree.FromPolygons(body.tolist(),body_faces,all_triangles=True)
        rows = []
        for i,point in enumerate(points):
            hit,_,_,distance = tree.find_nearest(Vector(point))
            if distance >= 1: continue
            direction = (point-np.asarray(hit))/max(distance,1e-9)
            reference_hit,_,_,reference_distance = tree.find_nearest(Vector(reference[i]))
            rows.append(dict(vertex=i,distance_cm=distance,dot=float(direction @ normals[i]),
                containment=containment(tree,point), reference_containment=containment(tree,reference[i]),
                reference_distance_cm=reference_distance, reference_error_cm=float(error[i])))
        case = dict(frame=frame,maximum_morphs=maximum,nearby=len(rows),
            inside=sum(r['containment']=='inside' for r in rows),
            ambiguous=sum(r['containment']=='ambiguous' for r in rows),
            outside_opposed=sum(r['containment']=='outside' and r['dot']<0 for r in rows),
            both_inside=sum(r['containment']=='inside' and r['reference_containment']=='inside' for r in rows),
            proxy_only_inside=sum(r['containment']=='inside' and r['reference_containment']=='outside' for r in rows),
            reference_error_max_cm=float(error.max()),reference_error_p95_cm=float(np.percentile(error,95)),rows=rows)
        cases.append(case)
        print(json.dumps({k:v for k,v in case.items() if k!='rows'}),flush=True)
output.write_text(json.dumps(dict(inputs={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
    for p in (mesh_path,proxy_path,motion_path)},cases=cases,
    scope='Sampled skinned proxy only, no cloth. Proxy morphs interpolated offline; normals use base normals with blended bone transforms, not regenerated morph normals.'),separators=(',',':'))+'\n')

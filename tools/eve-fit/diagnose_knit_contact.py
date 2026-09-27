"""Inspect the measured waist patch and native backstop inputs without changing assets."""
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector

work = Path(__file__).resolve().parents[2]/'work/eve26'
trial = work/'knit-cloth3'
out = trial/'waist-contact.json'
assert not out.exists()
mesh = json.loads((work/'knit-w2/knit.mesh.json').read_text())
proxy = json.loads((trial/'proxy.json').read_text())['slots']['Collar-1']
readback = json.loads((trial/'normals.json').read_text())['assets'][0]
motion = json.loads((trial/'trimmed.json').read_text())
probe = json.loads((trial/'trimmed6-probe/review.json').read_text())
point = np.asarray(next(r['point_m'] for r in probe['probe_hits'] if r['object']=='Eve Body'))*100
point[1] *= -1
snapshot = motion['frames'][6]['pose']['Snapshot']
entries = dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
bind,pose = [],[]
for bone in mesh['bones']:
    q=bone['rotation']
    local=Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    bind.append(bind[bone['parent']] @ local if bone['parent']>=0 else local)
    t=entries[bone['name']]
    local=Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
    pose.append(pose[bone['parent']] @ local if bone['parent']>=0 else local)
transforms = np.asarray([np.asarray(p @ b.inverted()) for p,b in zip(pose,bind,strict=True)])
lookup = {b['name']:i for i,b in enumerate(mesh['bones'])}
weights = np.asarray([[i,lookup[b],w] for i,rows in enumerate(proxy['weights']) for b,w in rows])
vi,bi,wt = weights[:,0].astype(int),weights[:,1].astype(int),weights[:,2]
rest = np.asarray(proxy['positions'])
native_normals = np.asarray(readback['physical_normals'])
assert native_normals.shape == rest.shape
skinned = np.zeros_like(rest)
normals = np.zeros_like(rest)
np.add.at(skinned,vi,(np.einsum('nij,nj->ni',transforms[bi,:3,:3],rest[vi])+transforms[bi,:3,3])*wt[:,None])
np.add.at(normals,vi,np.einsum('nij,nj->ni',transforms[bi,:3,:3],native_normals[vi])*wt[:,None])
normals /= np.linalg.norm(normals,axis=1)[:,None]
actual = np.asarray(motion['frames'][6]['positions_cm'])
distances = np.linalg.norm(skinned-point,axis=1)
rows = []
for i in np.argsort(distances)[:12]:
    delta = actual[i]-skinned[i]
    radius = readback['backstop_radii'][i]
    offset = readback['backstop_distances'][i]
    center = skinned[i]-(radius+offset)*normals[i]
    rows.append(dict(vertex=int(i),rest_cm=rest[i].tolist(),skinned_cm=skinned[i].tolist(),actual_cm=actual[i].tolist(),
        distance_to_patch_cm=float(distances[i]),displacement_cm=float(np.linalg.norm(delta)),
        displacement_along_normal_cm=float(delta @ normals[i]),
        normal_dot_patch_direction=float(normals[i] @ (skinned[i]-point)/max(distances[i],1e-8)),
        backstop_surface_margin_cm=float(np.linalg.norm(actual[i]-center)-radius),
        max_distance_cm=readback['max_distances'][i],normal=normals[i].tolist()))
report=dict(frame=6,body_patch_cm=point.tolist(),
    native_vs_proxy_normal_max=float(np.max(np.abs(native_normals-np.asarray(proxy['normals'])))),
    rows=rows,scope='Local measured patch, native saved normals and non-legacy backstop sphere equation. Direction toward the probe is not a general surface normal. No runtime constraint-order readback.')
out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)

"""Compare original and continuous skirt skinning on identical recorded poses."""
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector

work = Path(__file__).resolve().parents[2] / 'work/eve26'
out = work/'knit-w2/pose-edges.json'
assert not out.exists()
source = json.loads((work/'knit-export2/knit.mesh.json').read_text())
trial = json.loads((work/'knit-w2/knit.mesh.json').read_text())
proxy = json.loads((work/'knit-cloth1/proxy.json').read_text())['slots']['Collar-1']
audit = json.loads((work/'knit-export2/knit.mesh.audit.json').read_text())
start = audit['parts'][0]['points']
assert audit['parts'][1]['name'] == 'Eve Extras - Sweater'
count = audit['parts'][1]['points']
points = np.asarray(source['points'])[start:start+count]
pairs = set()
for face in source['faces']:
    ids = [source['wedges'][w][0]-start for w in face[:3]]
    if all(0 <= i < count for i in ids):
        pairs.update(tuple(sorted((ids[i],ids[(i+1)%3]))) for i in range(3))
edges = np.asarray(sorted(pairs))
rest = np.linalg.norm(points[edges[:,0]]-points[edges[:,1]],axis=1)
measured_edges = rest >= .05
bind = []
for bone in source['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']] @ local if bone['parent'] >= 0 else local)
weights = {}
for name, mesh in [('original', source), ('candidate', trial)]:
    rows = np.asarray([[v-start, b, w] for v,b,w in mesh['influences'] if start <= v < start+count])
    weights[name] = rows[:,0].astype(int), rows[:,1].astype(int), rows[:,2]
transfers = [proxy['transfer'][i] for i in (1071,1072)]
motion = json.loads((work/'knit-cloth1/sprint.json').read_text())
cases = []
for frame in motion['frames']:
    snap = frame['pose']['Snapshot']
    transforms = dict(zip(snap['BoneNames'], snap['LocalTransforms'], strict=True))
    pose = []
    for bone in source['bones']:
        t = transforms[bone['name']]
        local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']), Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[bone['parent']] @ local if bone['parent'] >= 0 else local)
    matrices = np.asarray([np.asarray(a @ b.inverted()) for a,b in zip(pose,bind,strict=True)])
    row = {'frame': frame['frame']}
    for name, (vi,bi,wt) in weights.items():
        posed = np.zeros_like(points)
        np.add.at(posed, vi, (np.einsum('nij,nj->ni', matrices[bi,:3,:3],points[vi])+matrices[bi,:3,3])*wt[:,None])
        ends = [np.asarray(t['barycentric']) @ posed[t['vertices']] for t in transfers]
        row[name+'_edge_cm'] = float(np.linalg.norm(ends[0]-ends[1]))
        lengths = np.linalg.norm(posed[edges[:,0]]-posed[edges[:,1]],axis=1)
        ratios = lengths[measured_edges]/rest[measured_edges]
        row[name+'_all_edges'] = dict(max_ratio=float(ratios.max()),
            p99_ratio=float(np.percentile(ratios,99)),over_double=int((ratios>2).sum()),
            max_extension_cm=float((lengths-rest).max()))
    cases.append(row)
report = dict(cases=cases, original_max_cm=max(r['original_edge_cm'] for r in cases),
    candidate_max_cm=max(r['candidate_edge_cm'] for r in cases),
    edge_count=len(edges),ratio_min_rest_length_cm=.05,
    scope='Diagnosed proxy correspondence and all garment triangle edges over 65 identical poses. Ratios exclude edges below 0.05 cm; absolute extension covers all edges. Skinning only, no cloth or contact acceptance.')
out.write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k!='cases'})

"""Compare fastener edge deformation on identical recorded bone poses."""
import hashlib
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector

w=Path(__file__).resolve().parents[2]/'work/eve26'
out=w/'planet-fastener-shape.json'
assert not out.exists()
before=json.loads((w/'planet-neck-trial/planet.mesh.json').read_text())
after=json.loads((w/'planet-fastener-trial/planet.mesh.json').read_text())
receipt=json.loads((w/'planet-fastener-trial/receipt.json').read_text())
ids={v for c in receipt['components'] for v in c['vertices']}
faces=[[before['wedges'][i][0] for i in f[:3]] for f in before['faces']]
edges=sorted({tuple(sorted((f[i],f[(i+1)%3]))) for f in faces if all(v in ids for v in f) for i in range(3)})
edges=np.asarray(edges)
base=np.asarray(before['points'])
length=np.linalg.norm(base[edges[:,0]]-base[edges[:,1]],axis=1)
edges=edges[length>.01];length=length[length>.01]
bind=[]
for b in before['bones']:
    q=b['rotation'];t=Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
    bind.append(bind[b['parent']]@t if b['parent']>=0 else t)
sources=[]
for mesh in [before,after]:
    inf=np.asarray([row for row in mesh['influences'] if row[0] in ids])
    sources.append((inf[:,0].astype(int),inf[:,1].astype(int),inf[:,2]))
motion_path=w/'planet-embedded-secondary.json'
motion=json.loads(motion_path.read_text());rows=[]
for frame in motion['frames']:
    snap=frame['pose']['Snapshot'];entries=dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True));pose=[]
    for b in before['bones']:
        v=entries[b['name']];t=Matrix.LocRotScale(Vector([v['Translation'][k] for k in 'XYZ']),Quaternion([v['Rotation'][k] for k in 'WXYZ']),Vector([v['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[b['parent']]@t if b['parent']>=0 else t)
    matrices=np.asarray([np.asarray(p@b.inverted()) for p,b in zip(pose,bind)])
    row={'frame':frame['frame']}
    for label,(vi,bi,wt) in zip(['before','after'],sources):
        points=np.zeros_like(base)
        np.add.at(points,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],base[vi])+matrices[bi,:3,3])*wt[:,None])
        ratio=np.linalg.norm(points[edges[:,0]]-points[edges[:,1]],axis=1)/length
        row[label]={'min_ratio':float(ratio.min()),'max_ratio':float(ratio.max()),'max_deviation':float(abs(ratio-1).max())}
    rows.append(row)
result={'motion_sha256':hashlib.sha256(motion_path.read_bytes()).hexdigest(),'frames':rows,'scope':'Fastener edge lengths across 65 recorded poses. Not body contact or gameplay acceptance.'}
out.write_text(json.dumps(result,indent=2)+'\n')
for label in ['before','after']:
    print(label,'worst edge deviation',max(r[label]['max_deviation'] for r in rows))

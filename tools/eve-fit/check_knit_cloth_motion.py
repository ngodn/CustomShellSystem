"""Compare native cloth particles with the same frame's skinned proxy."""
import hashlib
import argparse
import json
from pathlib import Path
import sys
import numpy as np
from mathutils import Matrix, Quaternion, Vector

work = Path(__file__).resolve().parents[2]/'work/eve26'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--case', default='sprint', choices=['sprint', 'no-body', 'no-backstop', 'no-both', 'trimmed', 'backstop15'])
parser.add_argument('--trial', type=int, choices=[1,2,3], default=1)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
source = work/f'knit-cloth{args.trial}'/f'{args.case}.json'
output = work/f'knit-cloth{args.trial}'/f'{args.case}-check.json'
assert not output.exists()
motion = json.loads(source.read_text())
mesh = json.loads((work/('knit-w2/knit.mesh.json' if args.trial >= 2 else 'knit-export2/knit.mesh.json')).read_text())
proxy = json.loads((work/f'knit-cloth{args.trial}/proxy.json').read_text())['slots']['Collar-1']
assert motion['asset'] == f'/Game/CSS/EveTest/SK_KCloth{args.trial}.SK_KCloth{args.trial}'
assert len(motion['frames']) == 65
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    bind.append(bind[bone['parent']] @ local if bone['parent'] >= 0 else local)
lookup = {b['name']:i for i,b in enumerate(mesh['bones'])}
weights = np.asarray([[i,lookup[name],w] for i,rows in enumerate(proxy['weights']) for name,w in rows])
vi,bi,wt = weights[:,0].astype(int),weights[:,1].astype(int),weights[:,2]
points = np.asarray(proxy['positions'])
limits = np.asarray(proxy['max_distances'])
triangles = np.asarray(proxy['indices']).reshape(-1,3)
edges = sorted({tuple(sorted((int(t[i]),int(t[(i+1)%3])))) for t in triangles for i in range(3)})
edges = np.asarray(edges)
rest_lengths = np.linalg.norm(points[edges[:,0]]-points[edges[:,1]],axis=1)
assert rest_lengths.min() > 1e-8
cases = []
for frame in motion['frames']:
    snap = frame['pose']['Snapshot']
    entries = dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True))
    pose = []
    for bone in mesh['bones']:
        t = entries[bone['name']]
        local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[bone['parent']] @ local if bone['parent'] >= 0 else local)
    matrices = np.asarray([np.asarray(a @ b.inverted()) for a,b in zip(pose,bind)])
    skinned = np.zeros_like(points)
    np.add.at(skinned,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],points[vi])+matrices[bi,:3,3])*wt[:,None])
    actual = np.asarray(frame['positions_cm'])
    assert actual.shape == points.shape and np.isfinite(actual).all()
    delta = np.linalg.norm(actual-skinned,axis=1)
    ratios = np.linalg.norm(actual[edges[:,0]]-actual[edges[:,1]],axis=1)/rest_lengths
    cases.append(dict(frame=frame['frame'],fixed_max_cm=float(delta[limits==0].max()),
        displacement_max_cm=float(delta.max()),over_limit_max_cm=float(np.maximum(delta-limits,0).max()),
        edge_ratio_max=float(ratios.max()),edge_ratio_p95=float(np.percentile(ratios,95))))
report = dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),cases=cases,
    diagnostic_no_body_collision=motion.get('diagnostic_no_body_collision',False),
    diagnostic_no_backstop=motion.get('diagnostic_no_backstop',False),
    diagnostic_backstop_radius_override_cm=motion.get('diagnostic_backstop_radius_override_cm',-1),
    fixed_max_cm=max(c['fixed_max_cm'] for c in cases),
    displacement_max_cm=max(c['displacement_max_cm'] for c in cases),
    over_limit_max_cm=max(c['over_limit_max_cm'] for c in cases),
    edge_ratio_max=max(c['edge_ratio_max'] for c in cases),
    scope='Particle coordinate and edge diagnostics, not rendered contact, morph support or game acceptance.')
output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='cases'}))

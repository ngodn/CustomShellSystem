"""Verify Chaos particle coordinates against the saved panel's own source weights."""
import json
import argparse
import sys
import hashlib
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector

work = Path(__file__).resolve().parents[2]/'work/eve26'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input',type=Path,default=work/'panel-motion.json')
p.add_argument('--output',type=Path,default=work/'panel-motion-verified.json')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
output = a.output
assert not output.exists()
mesh = json.loads((work/'holiday.mesh.json').read_text())
proxy = json.loads((work/'skirt-proxies.json').read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
result = json.loads(a.input.read_text())
motion = json.loads(Path(result['source_motion']).read_text())
points = np.asarray(proxy['positions'], dtype=float)
names = {b['name']: i for i, b in enumerate(mesh['bones'])}
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    m = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@m if bone['parent'] >= 0 else m)
inverse = [m.inverted() for m in bind]
rows = [(v, names[b], w) for v, influences in enumerate(proxy['weights']) for b, w in influences]
rows = np.asarray(rows)
vi, bi, weights = rows[:, 0].astype(int), rows[:, 1].astype(int), rows[:, 2]
alpha = np.clip((points[:, 2].max()-20.-points[:, 2])/12., 0., 1.)
distance = 18.*alpha**2*(3.-2.*alpha)
fixed = distance == 0
faces = np.asarray(proxy['indices']).reshape((-1, 3))
edges = sorted({tuple(sorted((int(t[i]), int(t[(i+1) % 3])))) for t in faces for i in range(3)})
edges = np.asarray(edges)
lengths = np.linalg.norm(points[edges[:, 0]]-points[edges[:, 1]], axis=1)
active = ((~fixed)[edges[:, 0]] | (~fixed)[edges[:, 1]]) & (lengths > .01)
reports = []
for row in result['frames']:
    frame = row['frame']
    snapshot = motion['frames'][frame]['pose']['Snapshot']
    entries = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
    pose = []
    for bone in mesh['bones']:
        e = entries[bone['name']]
        m = Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),
            Quaternion([e['Rotation'][k] for k in 'WXYZ']), Vector([e['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[bone['parent']]@m if bone['parent'] >= 0 else m)
    mats = np.asarray([np.asarray(m@b) for m, b in zip(pose, inverse)])
    expected = np.zeros_like(points)
    np.add.at(expected, vi, (np.einsum('nij,nj->ni', mats[bi, :3, :3], points[vi])+mats[bi, :3, 3])*weights[:, None])
    actual = np.asarray(row['positions_cm'])
    assert actual.shape == expected.shape and np.isfinite(actual).all()
    error = np.linalg.norm(actual-expected, axis=1)
    ratios = np.linalg.norm(actual[edges[:, 0]]-actual[edges[:, 1]], axis=1)[active]/lengths[active]
    active_ids = np.flatnonzero(active)
    worst_edges = []
    for local in np.argsort(ratios)[-8:][::-1]:
        edge_id = int(active_ids[local])
        endpoints = edges[edge_id]
        worst_edges.append({'vertices':endpoints.tolist(),
            'rest_cm':float(lengths[edge_id]), 'ratio':float(ratios[local]),
            'max_distance_cm':distance[endpoints].tolist(),
            'rest_positions_cm':points[endpoints].tolist(),
            'actual_positions_cm':actual[endpoints].tolist(),
            'skin_positions_cm':expected[endpoints].tolist(),
            'skin_ratio':float(np.linalg.norm(expected[endpoints[0]]-expected[endpoints[1]])/lengths[edge_id]),
            'weights':[proxy['weights'][int(v)] for v in endpoints]})
    reports.append({'frame':frame, 'worst_edges':worst_edges, 'fixed_max_cm':float(error[fixed].max()),
        'fixed_p95_cm':float(np.percentile(error[fixed],95)), 'max_from_skin_cm':float(error.max()),
        'edge_ratio_max':float(ratios.max()), 'edge_ratio_p95':float(np.percentile(ratios,95))})
passed = max(r['fixed_max_cm'] for r in reports) < .02
output.write_text(json.dumps({'scope':'Saved panel coordinates/order check using exactly zero-distance particles and the panel source weights. Edge ratios use the old source rest mesh. Not fitting, collision or game acceptance.',
    'source_result':str(a.input),'source_sha256':hashlib.sha256(a.input.read_bytes()).hexdigest(),
    'passed':passed,'zero_distance':int(fixed.sum()),'cases':reports},indent=2)+'\n')
print('Passed',passed,'worst fixed cm',max(r['fixed_max_cm'] for r in reports),'worst edge ratio',max(r['edge_ratio_max'] for r in reports))
assert passed, 'Resolve particle mapping or pose-space mismatch before rendering'

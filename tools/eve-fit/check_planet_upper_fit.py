"""Measure upper-suit vertex clearance against the unchanged posed body."""
import argparse
import hashlib
import json
import sys
from collections import Counter
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--mesh', type=Path, required=True)
p.add_argument('--audit', type=Path, required=True)
p.add_argument('--motion', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
raw = a.mesh.read_bytes()
m = json.loads(raw)
audit = json.loads(a.audit.read_text())
assert hashlib.sha256(raw).hexdigest() == audit['output_sha256']
nb, ns = (audit['parts'][i]['points'] for i in (0, 1))
nf = audit['parts'][0]['faces']
body_faces = [[m['wedges'][j][0] for j in f[:3]] for f in m['faces'][:nf]]
suit_faces = [[m['wedges'][j][0] for j in f[:3]] for f in m['faces'][nf:nf+audit['parts'][1]['faces']]]
edges = Counter(tuple(sorted((f[i], f[(i+1)%3]))) for f in suit_faces for i in range(3))
border = {v for edge, count in edges.items() if count == 1 for v in edge}
base = np.asarray(m['points'])
samples = [i for i in range(nb, nb+ns) if 115 < base[i, 2] < 160]
influences = np.asarray(m['influences'])
vi, bi, wt = influences[:, 0].astype(int), influences[:, 1].astype(int), influences[:, 2]
bind = []
for b in m['bones']:
    q = b['rotation']
    local = Matrix.LocRotScale(Vector(b['translation']), Quaternion((q[3], *q[:3])), Vector(b['scale']))
    bind.append(bind[b['parent']]@local if b['parent'] >= 0 else local)
motion = json.loads(a.motion.read_text())
cases = []
for frame in [None, 0, 16, 32, 53, 64]:
    posed = base.copy()
    if frame is not None:
        snapshot = motion['frames'][frame]['pose']['Snapshot']
        entries = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
        pose = []
        for b in m['bones']:
            t = entries[b['name']]
            local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),
                Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
            pose.append(pose[b['parent']]@local if b['parent'] >= 0 else local)
        matrices = np.asarray([np.asarray(p@b.inverted()) for p, b in zip(pose, bind)])
        posed[:] = 0
        np.add.at(posed, vi, (np.einsum('nij,nj->ni', matrices[bi, :3, :3], base[vi])+matrices[bi, :3, 3])*wt[:, None])
    xyz = [Vector(v) for v in posed*np.array([1, -1, 1])]
    tree = BVHTree.FromPolygons(xyz[:nb], body_faces, all_triangles=True)
    hits = []
    for i in samples:
        location, normal, face, distance = tree.find_nearest(xyz[i])
        signed = (xyz[i]-location).dot(normal)
        if signed < -.02 and distance < 1:
            hits.append(dict(vertex=i, border=i in border, signed_cm=signed, distance_cm=distance,
                             rest_cm=base[i].tolist(), body_face=face))
    hits.sort(key=lambda h: h['signed_cm'])
    cases.append(dict(frame=frame, hits=len(hits), border_hits=sum(h['border'] for h in hits), worst=hits[:30]))
result = dict(source_sha256=hashlib.sha256(raw).hexdigest(),
    motion_sha256=hashlib.sha256(a.motion.read_bytes()).hexdigest(), samples=len(samples), cases=cases,
    scope='Nearest outward body triangle clearance below -0.02 cm and within 1 cm. Local diagnostic, not closed-surface containment or fitting acceptance. No assets changed.')
a.output.write_text(json.dumps(result, indent=2)+'\n')
print([{k:v for k,v in case.items() if k != 'worst'} for case in cases])

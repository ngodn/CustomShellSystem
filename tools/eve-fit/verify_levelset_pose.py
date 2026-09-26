"""Independently reconstruct sampled body positions from source weights and recorded locals."""
import argparse
import json
import sys
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--source-mesh', type=Path, help='Mesh JSON matching the sampled positions and weights')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
query = json.loads(a.input.read_text())['bodies'][0]
mesh = json.loads((a.source_mesh or w/'cbody.mesh.json').read_text())
frame = query['sample_frame']
assert frame >= 0
snapshot = json.loads(Path(query['sample_motion']).read_text())['frames'][frame]['pose']['Snapshot']
recorded = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
bind, pose = [], []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@local if bone['parent'] >= 0 else local)
    t = recorded[bone['name']]
    local_pose = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),
        Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
    pose.append(pose[bone['parent']]@local_pose if bone['parent'] >= 0 else local_pose)
matrices = np.asarray([np.asarray(p@b.inverted()) for p, b in zip(pose, bind, strict=True)])
points = np.asarray(mesh['points'])
influences = np.asarray(mesh['influences'])
vertices, bones, weights = influences[:, 0].astype(int), influences[:, 1].astype(int), influences[:, 2]
expected = np.zeros_like(points)
np.add.at(expected, vertices, (np.einsum('nij,nj->ni', matrices[bones, :3, :3], points[vertices])+matrices[bones, :3, 3])*weights[:, None])
actual = np.asarray(query['sample_positions_cm'])
assert expected.shape == actual.shape
error = np.linalg.norm(expected-actual, axis=1)
maximum = float(error.max())
report = {'sample_frame': frame, 'points': len(points), 'max_position_error_cm': maximum,
          'passed': maximum < .001,
          'scope': 'Independent sampled body skinning check only; does not validate collider deformation or cloth contact.'}
a.output.write_text(json.dumps(report, indent=2)+'\n')
print(report)
assert report['passed'], report

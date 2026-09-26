"""Offline counterfactual: transfer nearby body surface weights to traced lattice corners."""
import argparse
import json
import sys
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
query = json.loads(a.input.read_text())['bodies'][0]
mesh = json.loads((w/'cbody.mesh.json').read_text())
body = json.loads((w/'body-collider.json').read_text())
snapshot = json.loads(Path(query['sample_motion']).read_text())['frames'][query['sample_frame']]['pose']['Snapshot']
recorded = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
bind, pose = [], []
names = {b['name']: i for i, b in enumerate(mesh['bones'])}
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@local if bone['parent'] >= 0 else local)
    t = recorded[bone['name']]
    local_pose = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),
        Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
    pose.append(pose[bone['parent']]@local_pose if bone['parent'] >= 0 else local_pose)
matrices = [np.asarray(p@b.inverted()) for p, b in zip(pose, bind, strict=True)]
positions = np.asarray(body['positions'])
tree = BVHTree.FromPolygons(positions.tolist(), body['indices'], all_triangles=True)
results = []
for trace in query['trace_samples']:
    assert not trace['empty_cell']
    corners = []
    predicted = np.zeros(3)
    replay_errors = []
    for corner in trace['corners']:
        point = np.asarray(corner['rest_cm'])
        replay = sum((weight*(matrices[names[name]]@np.append(point, 1))[:3] for name, weight in corner['weights']), np.zeros(3))
        replay_errors.append(float(np.linalg.norm(replay-corner['posed_cm'])))
        closest, normal, face_index, distance = tree.find_nearest(Vector(point))
        assert face_index is not None
        face = body['indices'][face_index]
        triangle = positions[face]
        uv = np.linalg.lstsq(np.column_stack((triangle[1]-triangle[0], triangle[2]-triangle[0])), np.asarray(closest)-triangle[0], rcond=None)[0]
        bary = np.asarray([1-uv.sum(), *uv])
        assert bary.min() > -1e-5 and bary.max() < 1.00001
        bary = np.clip(bary, 0, 1); bary /= bary.sum()
        weights = {}
        for vertex, amount in zip(face, bary, strict=True):
            for name, weight in body['weights'][vertex]:
                weights[name] = weights.get(name, 0)+float(amount)*weight
        ordered = sorted(((name, weight) for name, weight in weights.items() if weight > 1e-8), key=lambda item: -item[1])
        retained = ordered[:12]
        dropped = sum(weight for _, weight in ordered[12:])
        total = sum(weight for _, weight in retained)
        transferred = {name: weight/total for name, weight in retained}
        posed = sum((weight*(matrices[names[name]]@np.append(point, 1))[:3] for name, weight in transferred.items()), np.zeros(3))
        predicted += corner['barycentric_weight']*posed
        corners.append({'rest_cm': point.tolist(), 'nearest_body_face': face_index, 'surface_distance_cm': distance,
                        'dropped_weight': dropped, 'transferred_weights': transferred, 'posed_cm': posed.tolist()})
    original = float(np.linalg.norm(np.asarray(trace['lattice_cm'])-trace['skin_cm']))
    revised = float(np.linalg.norm(predicted-trace['skin_cm']))
    assert max(replay_errors) < .001, replay_errors
    results.append({'body_index': trace['body_index'], 'original_error_cm': original,
                    'hypothetical_error_cm': revised, 'hypothetical_cm': predicted.tolist(), 'corners': corners,
                    'max_original_corner_replay_error_cm': max(replay_errors)})
    print(trace['body_index'], 'original', original, 'local surface transfer', revised, flush=True)
a.output.write_text(json.dumps({'scope': 'Selected-corner offline counterfactual only. Same grid and body; no collider rebuilt, saved or accepted. Not full-surface validation.',
                               'input': str(a.input), 'sample_frame': query['sample_frame'], 'samples': results}, indent=2)+'\n')

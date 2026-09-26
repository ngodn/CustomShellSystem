"""Compare reconstructed animated backstops with native frame-end cloth positions."""
import argparse
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--motion', type=Path, required=True)
p.add_argument('--mapping', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
motion = json.loads(a.motion.read_text())
saved = json.loads(a.mapping.read_text())
assert motion['asset'] == saved['asset']
proxy = json.loads((w/'panel-follow-proxy.json').read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
body = json.loads((w/'body-collider.json').read_text())
mesh = json.loads((w/'holiday-waist-source.mesh.json').read_text())
source = json.loads(Path(motion['source_motion']).read_text())
names = {bone['name']: i for i, bone in enumerate(mesh['bones'])}
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@local if bone['parent'] >= 0 else local)

def deform(points, weights, transforms, normal=False):
    points = np.asarray(points)
    result = np.zeros_like(points)
    for i, row in enumerate(weights):
        for name, amount in row:
            matrix = transforms[names[name]]
            result[i] += amount*(matrix[:3, :3]@points[i]+(0 if normal else matrix[:3, 3]))
    if normal:
        result /= np.linalg.norm(result, axis=1)[:, None]
    return result

radius = np.asarray(saved['weight_maps']['BackstopRadius'])
offset = np.asarray(saved['weight_maps']['BackstopDistance'])
limits = np.asarray(saved['weight_maps']['MaxDistance'])
dynamic = limits >= .1
reports = []
for fi, frame in enumerate(motion['frames']):
    snapshot = source['frames'][fi]['pose']['Snapshot']
    entries = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
    pose = []
    for bone in mesh['bones']:
        t = entries[bone['name']]
        local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),
                                   Quaternion([t['Rotation'][k] for k in 'WXYZ']),
                                   Vector([t['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[bone['parent']]@local if bone['parent'] >= 0 else local)
    transforms = np.asarray([np.asarray(m@b.inverted()) for m, b in zip(pose, bind, strict=True)])
    animated = deform(proxy['positions'], proxy['weights'], transforms)
    normals = deform(proxy['normals'], proxy['weights'], transforms, normal=True)
    simulated = np.asarray(frame['positions_cm'])
    centers = animated-(radius+offset)[:, None]*normals
    sphere_clearance = np.linalg.norm(simulated-centers, axis=1)-radius
    row = {'frame': fi, 'dynamic_sphere_min_cm': float(sphere_clearance[dynamic].min()),
           'dynamic_inside_sphere_over_1mm': int((sphere_clearance[dynamic] < -.1).sum())}
    if fi in (0, 60, 64, 68):
        posed_body = deform(body['positions'], body['weights'], transforms)
        tree = BVHTree.FromPolygons(posed_body.tolist(), [[c, b, a] for a, b, c in body['indices']], all_triangles=True)
        details = []
        for i in np.flatnonzero(dynamic):
            near, normal, face, _ = tree.find_nearest(Vector(simulated[i]))
            signed = (Vector(simulated[i])-near).dot(normal)
            if signed >= -.1:
                continue
            weights = {}
            for vertex in body['indices'][face]:
                for name, amount in body['weights'][vertex]:
                    weights[name] = weights.get(name, 0.)+amount/3
            details.append({'particle': int(i), 'body_signed_cm': signed,
                            'sphere_clearance_cm': float(sphere_clearance[i]),
                            'normal_dot_body': float(np.dot(normals[i], normal)),
                            'normal_displacement_cm': float(np.dot(simulated[i]-animated[i], normals[i])),
                            'max_distance_cm': float(limits[i]), 'rest_cm': proxy['positions'][i],
                            'animated_cm': animated[i].tolist(), 'animated_normal': normals[i].tolist(),
                            'simulated_cm': simulated[i].tolist(), 'body_face_weights': weights})
        row['clipped_dynamic'] = sorted(details, key=lambda item: item['body_signed_cm'])
    reports.append(row)
result = {'scope': 'Offline reconstruction from authored proxy normals and recorded poses using UE weighted-vector skinning convention. Not solver animation-buffer readback. Frame-end sphere violations can reflect later constraints or different stored normals; verify before attributing a solver defect.',
          'motion': str(a.motion), 'mapping': str(a.mapping), 'frames': reports}
a.output.write_text(json.dumps(result, indent=2)+'\n')
print('Worst reconstructed sphere penetration', min(r['dynamic_sphere_min_cm'] for r in reports), flush=True)
print('Final clipped particles', reports[-1].get('clipped_dynamic', [])[:5], flush=True)

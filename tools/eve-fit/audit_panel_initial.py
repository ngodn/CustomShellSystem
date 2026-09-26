"""Compare reset skin targets and simulated particles with the native skin API surface."""
import argparse
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--frames', type=int, nargs='+', required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
result = json.loads(a.input.read_text())
body = json.loads(Path(result['body_collision_input']).read_text())
motion = json.loads(Path(result['source_motion']).read_text())
mesh = json.loads((w/'holiday.mesh.json').read_text())
panel = json.loads((w/'skirt-proxies.json').read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
names = {b['name']:i for i,b in enumerate(mesh['bones'])}
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    m = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3],*q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@m if bone['parent']>=0 else m)
rest = np.asarray(panel['positions'])
alpha = np.clip((rest[:,2].max()-20.-rest[:,2])/12.,0.,1.)
free = 18.*alpha**2*(3.-2.*alpha) >= .1
assert all(int(free.sum()) == result['frames'][frame]['dynamic_particles'] for frame in a.frames)
reports = []
for frame in a.frames:
    snap = motion['frames'][frame]['pose']['Snapshot']
    entries = dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True))
    pose = []
    for bone in mesh['bones']:
        t = entries[bone['name']]
        m = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[bone['parent']]@m if bone['parent']>=0 else m)
    matrices = [m@b.inverted() for m,b in zip(pose,bind)]
    target = np.zeros_like(rest)
    for index,row in enumerate(panel['weights']):
        for bone,weight in row:
            target[index] += np.asarray(matrices[names[bone]]@Vector(rest[index]))*weight
    # Chaos explicitly negates its UE triangle normal during contact resolution.
    tree = BVHTree.FromPolygons(result['frames'][frame]['body_reference_skin_cm'],
        [[c,b,a] for a,b,c in body['indices']],all_triangles=True)
    row = {'frame':frame}
    for label,points in [('skin_target',target),('simulated',np.asarray(result['frames'][frame]['positions_cm']))]:
        distances, signed = [],[]
        for point in points[free]:
            near,normal,_,distance = tree.find_nearest(Vector(point))
            distances.append(distance)
            signed.append((Vector(point)-near).dot(normal))
        signed = np.asarray(signed);distances = np.asarray(distances)
        row[label] = {'free_vertices':len(signed),'inside_over_1mm':int((signed<-.1).sum()),
            'inside_beyond_045cm_search':int(((signed<-.1)&(distances>.45)).sum()),
            'minimum_signed_cm':float(signed.min())}
    reports.append(row)
    print(row,flush=True)
a.output.write_text(json.dumps({'scope':'Nearest signed distances to native shape API skinning surface, not internal solver readback. Free vertices only. 0.45 cm is the 1.5x0.3 cm proximity range for this trial. Folded/concave surfaces can make nearest-normal signs ambiguous.',
    'input':str(a.input),'frames':reports},indent=2)+'\n')

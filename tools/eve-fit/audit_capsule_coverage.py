"""Measure the unchanged primitive collider against posed body and garment samples."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--recipe', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
mesh = json.loads((w/'holiday-waist-source.mesh.json').read_text())
body = json.loads((w/'body-collider.json').read_text())
recipe = json.loads(a.recipe.read_text())
motion = json.loads((w/'follow-sprint-base.json').read_text())
names = {b['name']: i for i, b in enumerate(mesh['bones'])}
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    m = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[bone['parent']]@m if bone['parent'] >= 0 else m)
base = np.asarray(mesh['points'])
body_ids = np.asarray(body['source_vertices'])
arm = np.asarray([sum(weight for name, weight in row if any(n in name for n in ('arm', 'hand', 'thumb', 'index', 'middle', 'ring', 'pinky'))) for row in body['weights']])
rest = np.asarray(body['positions'])
roi = np.flatnonzero((rest[:, 2] >= 90)&(rest[:, 2] <= 125)&(arm < .25))
assert len(roi) > 100
inf = np.asarray(mesh['influences'])
vi, bi, weights = inf[:, 0].astype(int), inf[:, 1].astype(int), inf[:, 2]
slot = mesh['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
garment_ids = sorted({mesh['wedges'][i][0] for f in mesh['faces'] if f[3] == slot for i in f[:3]
                      if 90 <= base[mesh['wedges'][i][0], 2] <= 125})
rows = []
for label, selected in [('default', {}), ('hip-waist', {'PBMHipSize': 1., 'PBMWaistWidth': 1.})]:
    points = base.copy()
    for morph in mesh['morph_targets']:
        amount = selected.get(morph['name'], 0)
        if amount:
            for vertex, *delta in morph['deltas']:
                points[vertex] += np.asarray(delta)*amount
    for frame in (0, 4, 8, 16, 32, 48, 64):
        snapshot = motion['frames'][frame]['pose']['Snapshot']
        entries = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
        pose = []
        for bone in mesh['bones']:
            t = entries[bone['name']]
            assert max(abs(t['Scale3D'][k]-1) for k in 'XYZ') < 1e-4
            m = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']), Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector((1, 1, 1)))
            pose.append(pose[bone['parent']]@m if bone['parent'] >= 0 else m)
        mats = np.asarray([np.asarray(m@b.inverted()) for m, b in zip(pose, bind, strict=True)])
        skinned = np.zeros_like(points)
        np.add.at(skinned, vi, (np.einsum('nij,nj->ni', mats[bi, :3, :3], points[vi])+mats[bi, :3, 3])*weights[:, None])
        spheres = [(np.asarray(pose[names[s['bone']]]@Vector(s['local_center_cm'])), s['radius_cm']) for s in recipe['spheres']]
        def phi(samples):
            values = [np.linalg.norm(samples-center, axis=1)-radius for center, radius in spheres]
            for connection in recipe['connections']:
                i, j = connection['sphere_indices']
                start, r0 = spheres[i]
                end, r1 = spheres[j]
                axis = end-start
                length = np.linalg.norm(axis)
                assert length > abs(r1-r0)
                axis /= length
                axial = (samples-start)@axis
                radial = np.linalg.norm(samples-start-axial[:, None]*axis, axis=1)
                slope = (r1-r0)/length
                along = np.clip(axial+slope*radial/np.sqrt(1-slope*slope), 0, length)
                values.append(np.hypot(axial-along, radial)-r0-slope*along)
            return np.min(values, axis=0)
        gaps = phi(skinned[body_ids[roi]])
        garment = phi(skinned[garment_ids])
        worst = []
        for i in np.argsort(gaps)[-8:][::-1]:
            index = int(roi[i])
            worst.append({'source_vertex': int(body_ids[index]), 'rest_cm': rest[index].tolist(),
                          'gap_cm': float(gaps[i]), 'weights': body['weights'][index]})
        bands = []
        for low, high in ((90, 105), (105, 118), (118, 125.001)):
            selected_ids = np.flatnonzero((rest[roi, 2] >= low)&(rest[roi, 2] < high))
            values = gaps[selected_ids]
            bands.append({'rest_z_cm': [low, high], 'samples': len(values),
                          'outside_over_3mm': int((values > .3).sum()),
                          'max_gap_cm': float(values.max()), 'p95_gap_cm': float(np.percentile(values, 95)),
                          'worst': [{'source_vertex': int(body_ids[roi[i]]), 'rest_cm': rest[roi[i]].tolist(),
                                     'gap_cm': float(gaps[i]), 'weights': body['weights'][roi[i]]}
                                    for i in selected_ids[np.argsort(values)[-4:][::-1]]]})
        rows.append({'case': label, 'frame': frame, 'body_samples': len(roi),
                     'body_outside_over_3mm': int((gaps > .3).sum()),
                     'body_gap_max_cm': float(gaps.max()), 'body_gap_p95_cm': float(np.percentile(gaps, 95)),
                     'skinned_garment_samples': len(garment_ids),
                     'skinned_garment_inside_over_1mm': int((garment < -.1).sum()),
                     'worst_body_samples': worst, 'height_bands': bands})
        print(label, frame, rows[-1]['body_gap_max_cm'], rows[-1]['body_outside_over_3mm'], flush=True)
a.output.write_text(json.dumps({'scope': 'Analytic sphere/tapered-capsule union versus body vertices in rest-Z90..125 excluding arm-dominated weights; seven sprint poses, default and combined hip/waist. Garment samples use skinning only. No collider morph adjustment, surface containment proof or native simulation.',
                               'recipe': str(a.recipe), 'recipe_sha256': hashlib.sha256(a.recipe.read_bytes()).hexdigest(),
                               'cases': rows}, indent=2)+'\n')

"""Compare retained Slipstream constraint captures against the same posed body.

Run with Blender 5.2.2. This is an oriented nearest-surface screen, not a
closed-volume collision proof or native render readback.
"""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import sys

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
EVE = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
INPUT = ROOT / 'work/three-outfit-rig-physics/slipstream-continuous-isolation2'
OUT = ROOT / 'work/three-outfit-rig-physics/slipstream-continuous-contact1'
PROXY = EVE / 'work/slipstream-cloth-continuous2/proxy.json'
MESH = EVE / 'work/slipstream-export1/slipstream-garments.mesh.json'
MAPPING = EVE / 'work/slipstream-cloth-native18/cloth-inspect.json'
BLEND = EVE / 'work/slipstream-jacketbind1.blend'
FRAMES = (0, 54, 119)
CASES = ('baseline', 'no_backstop', 'no_self')


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def measure(points, tree):
    signed = []
    for p in points:
        hit, normal, _, _ = tree.find_nearest(Vector(p))
        signed.append(float((Vector(p) - hit).dot(normal)))
    signed = np.asarray(signed)
    return dict(samples=len(signed), inward_over_1mm=int((signed < -.1).sum()),
                inward_over_5mm=int((signed < -.5).sum()), minimum_cm=float(signed.min()),
                p05_cm=float(np.percentile(signed, 5)),
                worst=[dict(sample=int(i), signed_cm=float(signed[i]), point_cm=points[i].tolist())
                       for i in np.argsort(signed)[:8]])


def main():
    global INPUT, OUT, PROXY, MAPPING, CASES
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=INPUT)
    parser.add_argument('--output', type=Path, default=OUT)
    parser.add_argument('--proxy', type=Path, default=PROXY)
    parser.add_argument('--mapping', type=Path, default=MAPPING)
    parser.add_argument('--cases', nargs='+', default=CASES)
    parser.add_argument('--verify-anchors', action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    INPUT, OUT, PROXY, MAPPING, CASES = args.input, args.output, args.proxy, args.mapping, args.cases
    OUT.mkdir(exist_ok=False)
    protected = {str(p): sha(p) for p in (PROXY, MESH, MAPPING, BLEND)}
    captures = {c: json.loads((INPUT / (c + '.json')).read_text()) for c in CASES}
    for case in CASES:
        if args.verify_anchors:
            sys.argv = ['verify_panel_motion.py', '--', '--input', str(INPUT / (case + '.json')),
                        '--output', str(INPUT / (case + '-anchors.json')), '--proxy', str(PROXY),
                        '--mesh', str(MESH), '--slot', 'Slipstream_skirt_Fabric',
                        '--kinematic-threshold', '.1']
            runpy.run_path(str(ROOT / 'tools/eve-fit/verify_panel_motion.py'), run_name='__main__')
        for frame in FRAMES:
            sys.argv = ['resolve_panel_render.py', '--', '--mesh', str(MESH),
                        '--slot', 'Slipstream_skirt_Fabric', '--mapping', str(MAPPING),
                        '--motion', str(INPUT / (case + '.json')), '--proxy', str(PROXY),
                        '--verification', str(INPUT / (case + '-anchors.json')),
                        '--frame', str(frame), '--output', str(OUT / f'{case}-{frame}-render.json')]
            runpy.run_path(str(ROOT / 'tools/eve-fit/resolve_panel_render.py'), run_name='__main__')
    sys.path.insert(0, str(EVE / 'tools'))
    from pose_peony_study import apply_pose
    from review_slipstream_binding import surface
    receipt = json.loads(BLEND.with_suffix('.json').read_text())
    assert sha(BLEND) == receipt['blend_sha256']
    bpy.ops.wm.open_mainfile(filepath=str(BLEND))
    body = bpy.data.objects[receipt['binding_study']['body']]
    proxy = json.loads(PROXY.read_text())['slots']['Slipstream_skirt_Fabric']
    mapping = json.loads(MAPPING.read_text())
    assert not mapping['assets'][0]['legacy_backstop']
    rest_normals = np.asarray(mapping['assets'][0]['physical_normals'])
    target = json.loads(MESH.read_text())
    bind = {}
    for b in target['bones']:
        q = b['rotation']
        m = Matrix.LocRotScale(Vector(b['translation']), Quaternion((q[3], *q[:3])), Vector(b['scale']))
        parent = target['bones'][b['parent']]['name'] if b['parent'] >= 0 else None
        bind[b['name']] = bind[parent] @ m if parent else m
    inverse = {n: m.inverted() for n, m in bind.items()}
    reports = []
    for frame in FRAMES:
        pose_receipt = apply_pose(receipt, INPUT / (CASES[0] + '.json'), frame, native_cloth=True)
        points, faces = surface(body)
        body_tree = BVHTree.FromPolygons((points * [100, -100, 100]).tolist(),
                                       faces[:, [0, 2, 1]].tolist(), all_triangles=True)
        snapshot = captures[CASES[0]]['frames'][frame]['pose']['Snapshot']
        entries = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
        pose, transforms = {}, {}
        for b in target['bones']:
            e = entries[b['name']]
            m = Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),
                Quaternion([e['Rotation'][k] for k in 'WXYZ']), Vector([e['Scale3D'][k] for k in 'XYZ']))
            parent = target['bones'][b['parent']]['name'] if b['parent'] >= 0 else None
            pose[b['name']] = pose[parent] @ m if parent else m
            transforms[b['name']] = np.asarray(pose[b['name']] @ inverse[b['name']])
        skinned, normals = [], []
        for p, n, influences in zip(proxy['positions'], rest_normals, proxy['weights'], strict=True):
            x, normal = np.zeros(3), np.zeros(3)
            for name, weight in influences:
                m = transforms[name]
                x += weight * (m[:3, :3] @ p + m[:3, 3])
                normal += weight * (m[:3, :3] @ n)
            skinned.append(x)
            normals.append(normal / np.linalg.norm(normal))
        skinned, normals = np.asarray(skinned), np.asarray(normals)
        radius = np.asarray(proxy['backstop_radii'])
        centers = skinned - (radius + np.asarray(proxy['backstop_distances']))[:, None] * normals
        for case in CASES:
            row = captures[case]['frames'][frame]
            assert row['pose']['Snapshot'] == snapshot
            actual = np.asarray(row['positions_cm'])
            render = json.loads((OUT / f'{case}-{frame}-render.json').read_text())['sections'][0]
            assert render['rest_mapping_max_cm'] < .03
            render_points = np.asarray(render['positions_cm'])
            triangles = np.asarray(render['indices']).reshape(-1, 3)
            samples = np.concatenate([render_points, render_points[triangles].mean(axis=1)])
            phi = np.linalg.norm(actual - centers, axis=1) - radius
            anchors = json.loads((INPUT / (case + '-anchors.json')).read_text())['cases'][frame]
            witnesses = []
            for edge in anchors['worst_edges'][:3]:
                ids = edge['vertices']
                witnesses.append(dict(vertices=ids, ratio=edge['ratio'],
                    backstop_phi_cm=phi[ids].tolist(),
                    particles=measure(actual[ids], body_tree),
                    backstop_centers_cm=centers[ids].tolist(), normals=normals[ids].tolist()))
            result = dict(case=case, frame=frame, pose=pose_receipt,
                          particles=measure(actual, body_tree),
                          visible_garment=measure(samples, body_tree),
                          skinned_particles=measure(skinned, body_tree),
                          reconstructed_backstop_inward_over_01cm=int((phi < -.1).sum()),
                          witnesses=witnesses)
            reports.append(result)
            print(case, frame, 'garment inward >5mm', result['visible_garment']['inward_over_5mm'],
                  'particle inward >5mm', result['particles']['inward_over_5mm'], flush=True)
    assert all(sha(Path(p)) == digest for p, digest in protected.items())
    (OUT / 'report.json').write_text(json.dumps(dict(scope=__doc__, protected=protected,
        recipe_sha256=sha(Path(__file__)), captures={c: sha(INPUT / (c + '.json')) for c in CASES},
        limitation='Backstops are reconstructed from saved weights/normals, not captured native constraint state. '
                   'Body distances are nearest oriented surfaces; no volume containment claim.',
        frames=reports), indent=2) + '\n')


if __name__ == '__main__':
    main()

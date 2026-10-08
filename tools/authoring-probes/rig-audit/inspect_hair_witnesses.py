"""Inspect selected hair-screen witnesses, without accepting open-mesh contact.

Run with Blender 5.2.2 Python. Winding values and ray escape counts describe
the supplied triangles; neither closes missing body surfaces or evaluates alpha.
"""
import argparse
import gzip
import json
from pathlib import Path
import sys

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, str(Path(__file__).resolve().parent))
from measure_hair_surfaces import global_matrices, prepare, sha, skin


def winding(point, triangles):
    a, b, c = (triangles[:, i] - point for i in range(3))
    la, lb, lc = (np.linalg.norm(v, axis=1) for v in (a, b, c))
    numerator = np.einsum('ij,ij->i', a, np.cross(b, c))
    denominator = (la * lb * lc + np.einsum('ij,ij->i', a, b) * lc
                   + np.einsum('ij,ij->i', b, c) * la
                   + np.einsum('ij,ij->i', c, a) * lb)
    return float(np.arctan2(numerator, denominator).sum() / (2 * np.pi))


def controls():
    vertices = np.array([[1, 1, 1], [-1, -1, 1], [-1, 1, -1], [1, -1, -1]], dtype=float)
    faces = np.array([[0, 2, 1], [0, 1, 3], [0, 3, 2], [1, 2, 3]])
    inside = winding(np.zeros(3), vertices[faces])
    outside = winding(np.array([4., 0, 0]), vertices[faces])
    assert abs(abs(inside) - 1) < 1e-12 and abs(outside) < 1e-12
    # A nonidentity rigid transform checks the row/column and weight conventions.
    group = dict(positions=np.array([[1., 2, 3]]), ids=np.array([[0, 1]]),
                 weights=np.array([[.25, .75]]))
    transforms = np.repeat(np.eye(4)[None], 2, axis=0)
    transforms[:, :3, :3] = [[0, -1, 0], [1, 0, 0], [0, 0, 1]]
    transforms[0, :3, 3] = [4, 5, 6]
    transforms[1, :3, 3] = [8, 9, 10]
    assert np.allclose(skin(group, transforms), [[5, 9, 12]], atol=1e-12, rtol=0)
    return dict(tetrahedron_inside_winding=inside, outside_winding=outside,
                weighted_rigid_skinning=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--screen', type=Path, required=True)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--solver', action='store_true', help='Compare solver output to its same-frame input')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists():
        raise FileExistsError(args.output)
    checked = controls()
    screen = json.loads(args.screen.read_text())
    for path, digest in screen['inputs'].items():
        if sha(Path(path)) != digest:
            raise ValueError('Screen input changed: ' + path)
    plan = json.loads((args.capture / 'input.json').read_text())
    report = json.loads((args.capture / 'report.json').read_text())
    if not report['passed']:
        raise ValueError('Target capture failed')
    capture_inputs = {str(args.capture / name): sha(args.capture / name) for name in ('input.json', 'report.json')}
    results = []
    for subject in report['results']:
        geometry_path = args.capture / subject['geometry']
        if sha(geometry_path) != subject['geometry_sha256']:
            raise ValueError('Target geometry changed')
        capture_inputs[str(geometry_path)] = subject['geometry_sha256']
        item = next(c for c in plan['cases'] if c['id'] == subject['id'])
        with gzip.open(args.capture / subject['geometry'], 'rt') as stream:
            geometry = json.load(stream)
        bones = geometry['bones']
        hair_indices = {i for i, b in enumerate(bones) if b['name'].startswith('CSS_Hair_')}
        groups = [prepare(g, hair_indices, g['slot'] in item['body_slots']) for g in geometry['groups']]
        inverse_bind = np.linalg.inv(global_matrices(bones, {}))
        for slot in item['hair_slots']:
            candidates = [r for r in screen['rows'] if r['subject'] == subject['id'] and r['slot'] == slot]
            metric = 'solver_max_added_depth_cm' if args.solver else 'max_new_depth_cm'
            vertex_key = 'solver_worst_vertex' if args.solver else 'worst_new_vertex'
            witness = max(candidates, key=lambda r: r[metric])
            if witness[metric] == 0:
                continue
            case = next(c for c in subject['cases'] if c['axis'] == witness['axis'])
            motion_path = args.capture / case['file']
            if sha(motion_path) != case['sha256']:
                raise ValueError('Target motion changed')
            capture_inputs[str(motion_path)] = case['sha256']
            with gzip.open(args.capture / case['file'], 'rt') as stream:
                motion = json.load(stream)
            comparisons = [(witness['frame'], False), (witness['frame'], True)] if args.solver else [(0, True), (witness['frame'], True)]
            for frame, solved in comparisons:
                sample = motion['samples'][frame]
                pose = {**sample.get('pose', {}), 'head': sample['head']}
                if solved:
                    pose.update(sample['hair'])
                transforms = global_matrices(bones, pose) @ inverse_bind
                body, faces, slots, offset = [], [], [], 0
                for group in groups:
                    if group['slot'] not in item['body_slots']:
                        continue
                    body.append(skin(group, transforms))
                    faces.append(group['faces'] + offset)
                    slots.extend([group['slot']] * len(group['faces']))
                    offset += len(body[-1])
                vertices, indices = np.concatenate(body), np.concatenate(faces)
                triangles = vertices[indices]
                tree = BVHTree.FromPolygons(vertices.tolist(), indices.tolist(), all_triangles=True)
                group = next(g for g in groups if g['slot'] == slot)
                point = skin(group, transforms)[witness[vertex_key]]
                nearest, normal, face, distance = tree.find_nearest(Vector(point))
                directions = np.concatenate((np.eye(3), -np.eye(3), [[.37, .51, .776]]))
                misses = sum(tree.ray_cast(Vector(point), Vector(d))[0] is None for d in directions)
                tri = triangles[face]
                uv = np.linalg.lstsq(np.stack((tri[1]-tri[0], tri[2]-tri[0]), axis=1),
                                    np.asarray(nearest)-tri[0], rcond=None)[0]
                results.append(dict(subject=subject['id'], slot=slot, axis=case['axis'], frame=frame,
                    solved=solved, vertex=witness[vertex_key], point_cm=point.tolist(),
                    nearest_slot=slots[face], nearest_cm=list(nearest), distance_cm=distance,
                    signed_plane_distance_cm=float((Vector(point)-nearest).dot(normal)),
                    nearest_barycentric=[float(1-uv.sum()), *uv.tolist()],
                    winding=winding(point, triangles), escaping_rays=int(misses), rays=len(directions)))
    output = dict(controls=checked, screen_sha256=sha(args.screen), inputs=screen['inputs'], capture_inputs=capture_inputs, rows=results,
                  comparison='same-frame solver input' if args.solver else 'frame zero',
                  scope='Worst added signed-nearest witness per hair section and its control. Open body geometry, no alpha or morphs. These diagnostics do not constitute gameplay acceptance.')
    for path, digest in {**screen['inputs'], **capture_inputs}.items():
        if sha(Path(path)) != digest:
            raise ValueError('Input changed during analysis: ' + path)
    args.output.write_text(json.dumps(output, indent=2, allow_nan=False) + '\n')
    print('HAIR_WITNESSES', len(results), flush=True)


if __name__ == '__main__':
    main()

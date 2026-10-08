"""Screen current weighted hair against posed body triangles using Blender 5.2.2.

Nearest-normal signs are a diagnostic near open/concave surfaces, not a proof
of inside/outside. Texture alpha, clothing and facial morphs are not evaluated.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def matrix(values):
    t = np.asarray(values, dtype=np.float64)
    x, y, z, w = t[3:7]
    if abs(np.linalg.norm(t[3:7]) - 1) > .0001:
        raise ValueError('Non-unit pose quaternion')
    m = np.eye(4)
    m[:3, :3] = np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                         [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                         [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]) * t[7:]
    m[:3, 3] = t[:3]
    return m


def global_matrices(bones, overrides):
    output = []
    for i, bone in enumerate(bones):
        parent = bone['parent']
        if parent >= i:
            raise ValueError('Bone hierarchy is not ordered')
        m = matrix(overrides.get(bone['name'], bone['local_transform']))
        output.append(output[parent] @ m if parent >= 0 else m)
    return np.array(output)


def prepare(group, hair_indices, body):
    count = len(group['vertices'])
    points = np.asarray(group['vertices'], dtype=np.float64)
    influence_count = max(map(len, group['weights']))
    ids = np.zeros((count, influence_count), dtype=np.int32)
    weights = np.zeros_like(ids, dtype=np.float64)
    for i, influences in enumerate(group['weights']):
        for j, (bone, value) in enumerate(influences):
            ids[i, j] = bone
            weights[i, j] = value / 65535
    if np.max(np.abs(weights.sum(axis=1)-1)) > 1e-12:
        raise ValueError('Unnormalized source weights')
    faces = np.array(group['indices'], dtype=np.int32).reshape(-1, 3)
    p = points[faces, :3]
    face_normals = np.cross(p[:, 1]-p[:, 0], p[:, 2]-p[:, 0])
    expected_normals = points[faces, 3:6].mean(axis=1)
    signs = np.einsum('ij,ij->i', face_normals, expected_normals)
    valid = np.abs(signs) > 1e-10
    negative = float((signs[valid] < 0).mean()) if valid.any() else 0
    if body and negative > .95:
        faces = faces[:, ::-1]
    elif body and negative > .05:
        raise ValueError('Ambiguous source winding: ' + group['slot'])
    return dict(slot=group['slot'], positions=points[:, :3], ids=ids, weights=weights,
                faces=faces, reversed_winding=body and negative > .95,
                hair_mask=(weights * np.isin(ids, list(hair_indices))).sum(axis=1) > 0)


def skin(group, transforms):
    p = group['positions']
    result = np.zeros_like(p)
    for j in range(group['ids'].shape[1]):
        m = transforms[group['ids'][:, j]]
        result += (np.einsum('nij,nj->ni', m[:, :3, :3], p) + m[:, :3, 3]) * group['weights'][:, j, None]
    if not np.isfinite(result).all():
        raise ValueError('Nonfinite skinning result')
    return result


def signed_nearest_depths(tree, points):
    depths = np.zeros(len(points))
    for j, point in enumerate(points):
        point = Vector(point)
        nearest, normal, _, distance = tree.find_nearest(point)
        if nearest is None:
            raise ValueError('Body query failed')
        if (point-nearest).dot(normal) < 0:
            depths[j] = distance
    return depths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    if args.output.exists():
        raise FileExistsError(args.output)
    args.output.mkdir(parents=True)
    capture = args.capture.resolve(strict=True)
    report = json.loads((capture/'report.json').read_text())
    if not report['passed']:
        raise ValueError('Motion capture failed')
    plan = json.loads((capture/'input.json').read_text())
    rows, inputs = [], {}
    for path in (capture/'report.json', capture/'input.json'):
        inputs[str(path)] = sha(path)
    for subject in report['results']:
        item = next(c for c in plan['cases'] if c['id'] == subject['id'])
        path = capture/subject['geometry']
        if sha(path) != subject['geometry_sha256']:
            raise ValueError('Geometry changed')
        inputs[str(path)] = sha(path)
        with gzip.open(path, 'rt') as handle:
            geometry = json.load(handle)
        bones = geometry['bones']
        hair_indices = {i for i, b in enumerate(bones) if b['name'].startswith('CSS_Hair_')}
        inverse_bind = np.linalg.inv(global_matrices(bones, {}))
        groups = [prepare(g, hair_indices, g['slot'] in item['body_slots']) for g in geometry['groups']]
        del geometry
        identity = global_matrices(bones, {}) @ inverse_bind
        rest_error = max(float(np.max(np.linalg.norm(skin(g, identity)-g['positions'], axis=1))) for g in groups)
        if rest_error > 1e-8:
            raise ValueError('Reference skinning does not reproduce input vertices')
        for case in subject['cases']:
            path = capture/case['file']
            if sha(path) != case['sha256']:
                raise ValueError('Motion changed')
            inputs[str(path)] = sha(path)
            with gzip.open(path, 'rt') as handle:
                motion = json.load(handle)
            baseline = {}
            for frame in (0, 30, 60, 90, 120, 180, 240):
                sample = motion['samples'][frame]
                if sample['frame'] != frame:
                    raise ValueError('Unexpected sample ordering')
                transforms = global_matrices(bones, {**sample.get('pose', {}), 'head': sample['head'], **sample['hair']}) @ inverse_bind
                unsolved = global_matrices(bones, {**sample.get('pose', {}), 'head': sample['head']}) @ inverse_bind
                body, triangles, offset = [], [], 0
                for g in groups:
                    if g['slot'] not in item['body_slots']:
                        continue
                    positions = skin(g, transforms)
                    if not np.allclose(positions, skin(g, unsolved), atol=1e-8, rtol=0):
                        raise ValueError('Hair solver affects the comparison body surface')
                    body.append(positions)
                    triangles.append(g['faces'] + offset)
                    offset += len(positions)
                tree = BVHTree.FromPolygons(np.concatenate(body).tolist(), np.concatenate(triangles).tolist(), all_triangles=True)
                for g in groups:
                    if g['slot'] not in item['hair_slots']:
                        continue
                    positions = skin(g, transforms)
                    indices = np.flatnonzero(g['hair_mask'])
                    if not len(indices):
                        raise ValueError('Hair section has no solver-weighted vertices')
                    depths = signed_nearest_depths(tree, positions[indices])
                    unsolved_depths = signed_nearest_depths(tree, skin(g, unsolved)[indices])
                    solver_added = np.maximum(0, depths-unsolved_depths)
                    solver_worst = int(indices[int(np.argmax(solver_added))])
                    if frame == 0:
                        baseline[g['slot']] = depths.copy()
                    added = np.maximum(0, depths-baseline[g['slot']])
                    worst = int(np.argmax(added))
                    index = int(indices[worst])
                    point = Vector(positions[index])
                    nearest, normal, face_index, distance = tree.find_nearest(point)
                    rows.append(dict(subject=item['id'], axis=case['axis'], frame=frame, slot=g['slot'],
                        vertices=len(indices), penetrating_over_1mm=int((depths > .1).sum()),
                        max_depth_cm=float(depths.max()), new_over_1mm=int((added > .1).sum()),
                        unsolved_max_depth_cm=float(unsolved_depths.max()),
                        solver_added_over_1mm=int((solver_added > .1).sum()),
                        solver_max_added_depth_cm=float(solver_added.max()),
                        solver_worst_vertex=solver_worst,
                        max_new_depth_cm=float(added.max()), worst_new_vertex=index,
                        worst_new_position_cm=list(point), nearest_body_cm=list(nearest),
                        nearest_body_normal=list(normal), source_winding_reversed=g['reversed_winding'],
                        reference_skinning_error_cm=rest_error))
                print('HAIR_SURFACE', item['id'], case['axis'], frame, flush=True)
                (args.output/'progress.json').write_text(json.dumps(rows[-len(item['hair_slots']):])+'\n')
    for path, digest in inputs.items():
        if sha(Path(path)) != digest:
            raise ValueError('Capture changed during analysis')
    (args.output/'report.json').write_text(json.dumps(dict(measured=True, rows=rows, inputs=inputs,
        scope='Seven selected frames per continuous 60 Hz case. All vertices with nonzero named hair-bone weight are queried against skinned body triangles, including a same-frame unsolved control. Nearest-normal signs can be ambiguous; texture alpha, garments, morph settings, intermediate-frame contact and gameplay are not accepted.'), indent=2)+'\n')


if __name__ == '__main__':
    main()

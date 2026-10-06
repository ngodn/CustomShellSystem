"""Transfer additional cooked head morphs onto an unchanged Eve facial export.

Run in Blender with --background --factory-startup --disable-autoexec.
Position/UV matches retain source deltas. At welded seams, incident triangles
select source corners and corner angles weight their contribution. Two unmatched
teeth points may project onto the teeth surface within a bounded distance.
This writes a candidate; cooking and performance review are separate checks.
"""
import argparse
import copy
import hashlib
import json
import math
import sys
from pathlib import Path

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import closest_point_on_tri
from mathutils.kdtree import KDTree


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def incident(triangles, count):
    result = [[] for _ in range(count)]
    for face, indices in enumerate(triangles):
        for index in indices:
            result[index].append(face)
    return result


def corner_weights(point, candidates, target, target_triangles, target_faces,
                   source, source_triangles, source_faces):
    weights = np.zeros(len(candidates))
    for face in target_faces[point]:
        triangle = target_triangles[face]
        center = Vector(target[triangle].mean(0))
        scores = []
        for candidate in candidates:
            scores.append(min((closest_point_on_tri(center,
                *(Vector(source[index]) for index in source_triangles[f])) - center).length
                for f in source_faces[candidate]))
        selected = np.flatnonzero(np.array(scores) <= min(scores) + 1e-5)
        edges = target[triangle[triangle != point]] - target[point]
        angle = float(np.arctan2(np.linalg.norm(np.cross(*edges)), np.dot(*edges)))
        weights[selected] += angle / len(selected)
    total = weights.sum()
    if not math.isfinite(total) or total <= 0:
        raise ValueError('Cannot bind an isolated or degenerate target corner')
    return weights / total


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--head-audit', type=Path, required=True)
    parser.add_argument('--body-samples', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--shape', action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists():
        raise ValueError('Use a new candidate directory')
    audit_path = args.head_audit / 'morph-audit.json'
    audit = json.loads(audit_path.read_text())
    files = {audit_path, args.body_samples, args.baseline}
    if {record['name'] for record in audit['files']} != {
            'morph-vertices.bin', 'morph-deltas.bin', 'morph-indices.bin'} or len(audit['files']) != 3:
        raise ValueError('Expected the three checksummed morph payloads')
    for record in audit['files']:
        if Path(record['name']).name != record['name']:
            raise ValueError('Unexpected audit filename')
        path = args.head_audit / record['name']
        if path.stat().st_size != record['bytes'] or digest(path) != record['sha256']:
            raise ValueError('Head audit payload changed')
        files.add(path)
    protected = {str(path.resolve()): digest(path) for path in files}
    if audit['schema'] != 1 or audit['vertex_stride'] != 20 or audit['delta_stride'] != 28:
        raise ValueError('Unsupported morph audit format')
    with np.load(args.body_samples, allow_pickle=False) as samples:
        body = {key: samples[key] for key in samples.files}
    target = body['points'].astype(float) * 100
    triangles, materials = body['triangles'], body['materials']
    used = body['used_points']
    baseline = json.loads(args.baseline.read_text())
    if (target.shape != (36789, 3) or used.shape != (36787,) or
            not np.array_equal(used, np.unique(used)) or
            not np.issubdtype(used.dtype, np.integer) or used.min() < 0 or used.max() >= len(target) or
            len(baseline['body_points']) != len(used) or
            not np.isfinite(target).all() or
            not np.allclose(body['matrix_world'], np.eye(4), atol=1e-8)):
        raise ValueError('Expected the audited Eve body topology and identity transform')
    if (triangles.ndim != 2 or triangles.shape[1] != 3 or triangles.size == 0 or
            not np.issubdtype(triangles.dtype, np.integer) or triangles.min() < 0 or
            triangles.max() >= len(target) or materials.shape != (len(triangles),) or
            body['triangle_uv'].shape != (len(triangles), 3, 2) or
            not np.isfinite(body['triangle_uv']).all() or
            body['fitted_points'].shape != (len(used), 3)):
        raise ValueError('Invalid captured target topology or UVs')
    error = float(np.linalg.norm(body['fitted_points'] - baseline['body_points'], axis=1).max())
    if not math.isfinite(error) or error > .0002:
        raise ValueError('Fitted body differs from the accepted facial export')
    old_names = {row['name'].casefold() for row in baseline['morph_targets']}
    names = args.shape
    if (len({name.casefold() for name in names}) != len(names) or
            any(name.casefold() in old_names for name in names) or
            len(old_names) + len(names) > 128):
        raise ValueError('Repeated, existing or excessive candidate morph names')
    raw = np.fromfile(args.head_audit/'morph-vertices.bin', dtype='<f4').reshape(-1, 5)
    source = raw[:, :3][:, [1, 0, 2]].astype(float)
    source_triangles = np.fromfile(args.head_audit/'morph-indices.bin', dtype='<u4').reshape(-1, 3)
    if (len(source) != audit['vertex_count'] or source_triangles.max() >= len(source)
            or not np.isfinite(raw).all()):
        raise ValueError('Invalid source geometry')
    section = audit['sections'][2]
    eyes = np.arange(section['BaseVertexIndex'], section['BaseVertexIndex'] + section['NumVertices'])
    target_eyes = np.unique(triangles[materials == 8])
    if len(eyes) != 1604 or len(target_eyes) != len(eyes):
        raise ValueError('Eye topology does not establish the alignment')
    scale = float(np.linalg.norm(np.ptp(target[target_eyes], axis=0)) /
                  np.linalg.norm(np.ptp(source[eyes], axis=0)))
    offset = target[target_eyes].mean(0) - source[eyes].mean(0)*scale
    source = source*scale + offset
    tree = KDTree(len(source))
    for index, point in enumerate(source):
        tree.insert(point, index)
    tree.balance()
    source_delta = np.zeros((len(names), len(source), 3))
    dtype = np.dtype([('index', '<u4'), ('position', '<f4', (3,)), ('normal', '<f4', (3,))])
    blob = (args.head_audit/'morph-deltas.bin').read_bytes()
    found = set()
    for record in audit['morphs']:
        if record['name'] not in names:
            continue
        if record['name'] in found:
            raise ValueError('Duplicate source morph')
        found.add(record['name'])
        values = np.frombuffer(blob, dtype, record['count'], record['offset_bytes'])
        if (len(np.unique(values['index'])) != len(values) or
                values['index'].max() >= len(source) or not np.isfinite(values['position']).all()):
            raise ValueError('Invalid source morph records')
        source_delta[names.index(record['name']), values['index']] = values['position'][:, [1, 0, 2]]*scale
    if found != set(names):
        raise ValueError('Missing requested source shapes')
    target_uvs = [set() for _ in target]
    for face, uvs in zip(triangles, body['triangle_uv'], strict=True):
        for index, uv in zip(face, uvs, strict=True):
            target_uvs[index].add(tuple(uv))
    source_faces = incident(source_triangles, len(source))
    target_faces = incident(triangles, len(target))
    facial = np.unique(triangles[np.isin(materials, [0, 8, 9, 10, 11, 12, 13, 14, 15])])
    result = np.zeros((len(names), len(target), 3))
    unresolved, bindings, welds = [], [], []
    for index in facial:
        candidates = sorted(hit[1] for hit in tree.find_range(target[index], .002))
        if not candidates:
            unresolved.append(int(index))
            continue
        uvs = np.array(list(target_uvs[index])); uvs[:, 1] = 1 - uvs[:, 1]
        distances = np.linalg.norm(raw[candidates, 3:, None] - uvs.T[None, :, :], axis=1).min(axis=1)
        if distances.min() > .0003:
            raise ValueError(f'No verified position/UV correspondence for point {index}')
        selected = np.array(candidates)[distances <= distances.min() + 1e-7]
        delta = source_delta[:, selected]
        difference = float(np.linalg.norm(delta - delta[:, 0, None], axis=2).max())
        weights = np.zeros(len(selected)); weights[0] = 1
        if len(selected) > 1 and difference > 1e-5:
            weights = corner_weights(index, selected, target, triangles, target_faces,
                                     source, source_triangles, source_faces)
            welds.append(int(index))
        result[:, index] = np.einsum('k,nkd->nd', weights, delta)
        bindings.append(dict(point=int(index), indices=selected.tolist(), weights=weights.tolist()))
    teeth_points = set(np.unique(triangles[materials == 14]))
    section = audit['sections'][7]
    mask = ((source_triangles >= section['BaseVertexIndex']) &
            (source_triangles < section['BaseVertexIndex'] + section['NumVertices'])).all(1)
    teeth = source_triangles[mask]
    tree = BVHTree.FromPolygons(source.tolist(), teeth.tolist(), all_triangles=True)
    projected, stationary = [], []
    for index in unresolved:
        if index not in teeth_points:
            if np.max(np.abs(body['deltas'][:, index])) != 0:
                raise ValueError('Unmapped target point has facial deformation')
            stationary.append(index)
            continue
        point, _, face, distance = tree.find_nearest(Vector(target[index]))
        if point is None or distance > .02:
            raise ValueError('Unmatched tooth is too far from its source surface')
        indices = teeth[face]; positions = source[indices]
        basis = np.stack((positions[0]-positions[2], positions[1]-positions[2]), axis=1)
        weight = np.linalg.lstsq(basis, np.array(point)-positions[2], rcond=None)[0]
        weights = np.append(weight, 1-weight.sum())
        if min(weights) < -1e-4 or max(weights) > 1.0001:
            raise ValueError('Invalid tooth surface coordinates')
        result[:, index] = np.einsum('k,nkd->nd', weights, source_delta[:, indices])
        projected.append(dict(point=index, distance_cm=distance))
        bindings.append(dict(point=index, indices=indices.tolist(), weights=weights.tolist()))
    if len(projected) != 2 or len(stationary) != 44:
        raise ValueError('Head/body correspondence no longer matches the audited topology')
    nonface = np.unique(triangles[~np.isin(materials, [0, 8, 9, 10, 11, 12, 13, 14, 15])])
    if not np.isfinite(result).all() or np.max(np.abs(result[:, nonface])) != 0:
        raise ValueError('Transfer changed nonfacial points')
    output = copy.deepcopy(baseline)
    for name, deltas in zip(names, result[:, used], strict=True):
        converted = deltas * np.array([1, -1, 1])
        active = np.flatnonzero(np.linalg.norm(converted, axis=1) > 1e-6)
        if len(active) == 0:
            raise ValueError('Transferred morph is empty')
        output['morph_targets'].append(dict(name=name, source_shape=name, baked_value=0,
            deltas=[[int(index), *converted[index].tolist()] for index in active]))
    if any(digest(Path(path)) != expected for path, expected in protected.items()):
        raise ValueError('A transfer input changed')
    args.output.mkdir(parents=True)
    payload = args.output/'facial-shapes.json'
    payload.write_text(json.dumps(output, separators=(',', ':'), allow_nan=False)+'\n')
    report = dict(candidate=True, installed=False, sources=protected, added_shapes=names,
        retained_shapes=len(baseline['morph_targets']), scale=scale, offset_cm=offset.tolist(),
        max_neutral_difference_cm=error, unchanged_nonface=True, stationary_neck_points=stationary,
        welded_points=welds, projected_teeth=projected, bindings=bindings,
        output_sha256=digest(payload), scope='Offline transfer. Original shapes and expressions retained; cook and live review pending.')
    (args.output/'report.json').write_text(json.dumps(report, separators=(',', ':'), allow_nan=False)+'\n')
    print(f'HEAD_TRANSFER_CANDIDATE: {len(names)} added; {len(old_names)} retained')


if __name__ == '__main__':
    main()

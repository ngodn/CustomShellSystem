"""Compare import geometry, weights and morphs with independent cooked readback.

Python 3.14 with NumPy. Requires separate MeshExport skin and morph audits.
Split render vertices are matched by exact float32 position, UV0 and material.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path

import numpy as np


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_audit(directory, mode, inputs):
    path = directory / (mode + '-audit.json')
    inputs[str(path)] = sha(path)
    report = json.loads(path.read_text())
    for file in report['files']:
        path = directory / file['name']
        if path.stat().st_size != file['bytes'] or sha(path) != file['sha256']:
            raise ValueError('Readback payload changed: ' + str(path))
        inputs[str(path)] = file['sha256']
    return report


def oriented_triangle(ids):
    a, b, c = map(int, ids)
    return min((a, b, c), (b, c, a), (c, a, b))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'skin', 'morph', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs = {str(args.source): sha(args.source), str(Path(__file__)): sha(Path(__file__))}
    source = json.loads(args.source.read_text())
    skin = read_audit(args.skin, 'skin', inputs)
    morph = read_audit(args.morph, 'morph', inputs)
    raw_path = args.morph / 'source-mesh.json'
    inputs[str(raw_path)] = sha(raw_path)
    raw = json.loads(raw_path.read_text())
    precision = raw['LODModels'][0]['MorphTargetVertexInfoBuffers']['PositionPrecision']
    if not 0 < precision < 0.1:
        raise ValueError('Unexpected morph precision')
    if skin['asset'] != morph['asset'] or skin['material_slots'] != source['materials']:
        raise ValueError('Asset or material-slot mismatch')
    if [(b['name'], b['parent']) for b in skin['bones']] != [(b['name'], b['parent']) for b in source['bones']]:
        raise ValueError('Bone names/order/parents changed')
    bind_errors = {key: float(np.max(np.abs(np.asarray([b[key] for b in skin['bones']]) -
                                           np.asarray([b[key] for b in source['bones']]))))
                   for key in ('translation', 'rotation', 'scale')}
    if bind_errors['translation'] > 0.0001 or max(bind_errors['rotation'], bind_errors['scale']) > 0.000001:
        raise ValueError('Bind transforms differ beyond float conversion')
    weight_dtype = np.dtype([('bone', '<u2'), ('raw', '<u2'), ('weight', '<f4')])
    vertex_dtype = np.dtype([('position', '<f4', (3,)), ('weights', weight_dtype, (skin['max_influences'],))])
    vertices = np.fromfile(args.skin / 'skin-vertices.bin', dtype=vertex_dtype)
    morph_vertices = np.fromfile(args.morph / 'morph-vertices.bin', dtype='<f4').reshape(-1, 5)
    if (vertex_dtype.itemsize != skin['vertex_stride'] or len(vertices) != skin['vertex_count'] or
        len(morph_vertices) != morph['vertex_count'] or not np.array_equal(vertices['position'], morph_vertices[:, :3])):
        raise ValueError('Skin and morph readbacks do not share render vertex order')
    sums = np.unique(vertices['weights']['raw'].sum(axis=1))
    if len(sums) != 1 or int(sums[0]) not in (255, 65535):
        raise ValueError('Cooked raw weights are not consistently normalized')
    weight_denominator = int(sums[0])
    if not np.allclose(vertices['weights']['weight'], vertices['weights']['raw'] / weight_denominator, rtol=0, atol=1e-7):
        raise ValueError('Float and raw cooked weights disagree')

    positions = np.asarray(source['points'], dtype='<f4')
    wedges = source['wedges']
    keys, candidates, source_faces = {}, defaultdict(set), []
    for face in source['faces']:
        triangle = []
        for wedge_index in face[:3]:
            wedge = wedges[wedge_index]
            point = int(wedge[0])
            key = (int(face[3]), *map(float, positions[point]), *map(float, np.asarray(wedge[1:3], dtype='<f4')))
            identity = keys.setdefault(key, len(keys))
            candidates[identity].add(point)
            triangle.append(identity)
        source_faces.append(oriented_triangle(triangle))
    slots = np.full(len(vertices), -1, dtype=np.int32)
    for section in morph['sections']:
        begin, count = section['BaseVertexIndex'], section['NumVertices']
        if begin < 0 or count < 0 or begin + count > len(slots) or np.any(slots[begin:begin + count] != -1):
            raise ValueError('Invalid section range')
        slots[begin:begin + count] = section['MaterialIndex']
    if np.any(slots == -1):
        raise ValueError('Sections do not cover render vertices')
    identities = np.empty(len(vertices), dtype=np.int32)
    for i, vertex in enumerate(morph_vertices):
        key = (int(slots[i]), *map(float, vertex))
        if key not in keys:
            raise ValueError(f'Cooked position/UV/material has no source corner: {i}')
        identities[i] = keys[key]
    indices = np.fromfile(args.morph / 'morph-indices.bin', dtype='<u4')
    if not np.array_equal(indices, np.fromfile(args.skin / 'skin-indices.bin', dtype='<u4')):
        raise ValueError('Skin and morph triangle order differs')
    if np.any(indices >= len(vertices)) or len(indices) % 3:
        raise ValueError('Invalid cooked triangles')
    cooked_faces = Counter(oriented_triangle(t) for t in identities[indices].reshape(-1, 3))
    if cooked_faces != Counter(source_faces):
        raise ValueError('Cooked triangle multiplicity or winding changed')

    source_weights = defaultdict(dict)
    for point, bone, weight in source['influences']:
        if bone in source_weights[point]:
            raise ValueError('Duplicate source influence')
        source_weights[point][bone] = weight
    names = [m['name'] for m in morph['morphs']]
    if len(set(names)) != len(names) or set(names) != {m['name'] for m in source['morph_targets']}:
        raise ValueError('Morph identities changed')
    expected = np.zeros((len(positions), len(names), 3), dtype=np.float64)
    for target in source['morph_targets']:
        column = names.index(target['name'])
        for point, x, y, z in target['deltas']:
            expected[point, column] = (x, y, z)
    actual = np.zeros((len(vertices), len(names), 3), dtype=np.float64)
    present = np.zeros((len(vertices), len(names)), dtype=bool)
    delta_dtype = np.dtype([('index', '<u4'), ('delta', '<f4', (3,)), ('normal', '<f4', (3,))])
    data = (args.morph / 'morph-deltas.bin').read_bytes()
    for column, target in enumerate(morph['morphs']):
        rows = np.frombuffer(data, dtype=delta_dtype, count=target['count'], offset=target['offset_bytes'])
        if np.any(rows['index'] >= len(vertices)) or len(set(map(int, rows['index']))) != len(rows):
            raise ValueError('Invalid morph vertex indices')
        actual[rows['index'], column] = rows['delta']
        present[rows['index'], column] = True
    # UE 5.6.1 stores integer weights and normalizes with carried rounding.
    # The importer leaves MorphThresholdPosition at its 0.015 cm default.
    stored_delta_limit = float(precision) + 0.00005
    missing_delta_limit = 0.015 + stored_delta_limit
    limits = np.where(present, stored_delta_limit, missing_delta_limit)
    max_weight_error, max_delta_error, max_omitted_delta = 0.0, 0.0, 0.0
    matched_points, ambiguous_vertices = set(), 0
    cache = {}
    for i, identity in enumerate(identities):
        weights = tuple((int(w['bone']), float(w['weight'])) for w in vertices[i]['weights'] if w['raw'])
        cache_key = (int(identity), weights, actual[i].tobytes(), present[i].tobytes())
        if cache_key in cache:
            matched_points.update(cache[cache_key])
            continue
        choices = []
        cooked_weights = dict(weights)
        for point in candidates[int(identity)]:
            original = source_weights[point]
            weight_error = max(abs(original.get(b, 0) - cooked_weights.get(b, 0)) for b in original.keys() | cooked_weights.keys())
            weight_limit = len(original) / weight_denominator + 0.0000002
            error = np.abs(expected[point] - actual[i])
            if weight_error <= weight_limit and np.all(error <= limits[i, :, None]):
                choices.append((point, weight_error, error))
        if not choices:
            raise ValueError(f'No source vertex matches cooked weights and every morph at render vertex {i}')
        ambiguous_vertices += len(choices) > 1
        points = {choice[0] for choice in choices}
        matched_points.update(points)
        cache[cache_key] = points
        for point, weight_error, error in choices:
            max_weight_error = max(max_weight_error, weight_error)
            max_delta_error = max(max_delta_error, float(np.max(error[present[i]], initial=0)))
            max_omitted_delta = max(max_omitted_delta, float(np.max(np.abs(expected[point, ~present[i]]), initial=0)))
    used = {int(wedges[w][0]) for f in source['faces'] for w in f[:3]}
    if used != matched_points:
        raise ValueError('Not all used import points have a cooked correspondence')
    if any(sha(Path(p)) != checksum for p, checksum in inputs.items()):
        raise ValueError('Input changed during comparison')
    report = dict(passed=True, asset=skin['asset'], source_points=len(positions), used_source_points=len(used),
                  cooked_vertices=len(vertices), triangles=len(indices) // 3, bones=len(skin['bones']), morphs=names,
                  bind_component_errors=bind_errors, exact_position_uv_material_keys=True,
                  triangle_multiplicity_and_winding_equal=True, max_weight_error=max_weight_error,
                  weight_denominator=weight_denominator,
                  weight_error_bound='source influence count / weight_denominator + 0.0000002',
                  stored_delta_component_limit_cm=stored_delta_limit, missing_delta_component_limit_cm=missing_delta_limit,
                  max_stored_delta_component_error_cm=max_delta_error, max_omitted_source_delta_component_cm=max_omitted_delta,
                  ambiguous_corner_attribute_groups=ambiguous_vertices, inputs=inputs,
                  scope='LOD0 import-to-cooked position, UV0, topology, bind, named weight and positional-morph correspondence. '
                        'Includes bounded weight/morph quantization and morph omission. No normals, tangents, other UV channels, '
                        'colors, materials/shading, facial replacement, cloth, posed fit or gameplay claim.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'inputs'}))


if __name__ == '__main__':
    main()

"""Compare imported corner normals, every UV channel and colors to a cooked audit.

Python 3.14 with NumPy. Tangents are reported, not declared source-identical:
the importer generates them and the interchange does not contain tangents.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path

import numpy as np

from check_import_cooked_mesh import oriented_triangle, read_audit, sha

# UE packs signed normals at 1/127; this reader exposes offset bytes at 1/127.5.
# Allow their maximum conversion discrepancy plus half a packing step and float
# normalization, without treating the reader's offset as a game asset defect.
NORMAL_COMPONENT_LIMIT = 0.012


def compare(source, positions, surface, indices, sections):
    wedges = np.asarray(source['wedges'], dtype=np.float64)
    points = np.asarray(source['points'], dtype='<f4')
    normals = np.asarray(source['normals'], dtype='<f4')
    colors = np.asarray(source['colors'], dtype=np.uint8)
    uvs = wedges[:, 1:].astype('<f4')
    if not all(np.all(np.isfinite(a)) for a in (wedges, points, normals, positions,
                                               surface['normal'], surface['tangent'], surface['uv'])):
        raise ValueError('Nonfinite source or cooked surface field')
    if (len(normals) != len(wedges) or colors.shape != (len(wedges), 4) or
            uvs.shape != (len(wedges), source['uv_channels'] * 2) or
            surface['uv'].shape[1:] != (source['uv_channels'], 2) or
            surface['color'].shape[1:] != (1, 4)):
        raise ValueError('Unexpected surface channel layout')
    if np.max(np.abs(np.linalg.norm(normals, axis=1) - 1)) > 0.001:
        raise ValueError('Source normals are not normalized')
    source_keys, candidates, source_faces = {}, defaultdict(set), []
    for face in source['faces']:
        identities = []
        for wedge in face[:3]:
            point = int(wedges[wedge, 0])
            key = (int(face[3]), *map(float, points[point]), *map(float, uvs[wedge]), *map(int, colors[wedge]))
            identity = source_keys.setdefault(key, len(source_keys))
            candidates[identity].add(tuple(map(float, normals[wedge])))
            identities.append(identity)
        source_faces.append(oriented_triangle(identities))
    slots = np.full(len(positions), -1, dtype=np.int32)
    covered = np.zeros(len(indices), dtype=bool)
    for section in sections:
        start, stop = section['FirstIndex'], section['FirstIndex'] + section['NumFaces'] * 3
        if start < 0 or stop > len(indices) or np.any(covered[start:stop]):
            raise ValueError('Invalid section index ranges')
        covered[start:stop] = True
        section_vertices = indices[start:stop]
        previous = slots[section_vertices]
        if np.any((previous != -1) & (previous != section['MaterialIndex'])):
            raise ValueError('Vertex is shared across material sections')
        slots[section_vertices] = section['MaterialIndex']
    if not np.all(covered) or np.any(slots < 0):
        raise ValueError('Sections do not cover all indices and vertices')
    identities = np.full(len(positions), -1, dtype=np.int32)
    missing, normal_failures, max_error = [], [], 0.0
    cache = {}
    for i, position in enumerate(positions):
        key = (int(slots[i]), *map(float, position), *map(float, surface['uv'][i].flat),
               *map(int, surface['color'][i, 0]))
        identity = source_keys.get(key)
        if identity is None:
            missing.append(i)
            continue
        identities[i] = identity
        normal = tuple(map(float, surface['normal'][i, :3]))
        cache_key = (identity, normal)
        if cache_key not in cache:
            cache[cache_key] = min(max(abs(a-b) for a, b in zip(normal, candidate))
                                  for candidate in candidates[identity])
        error = cache[cache_key]
        max_error = max(max_error, error)
        if error > NORMAL_COMPONENT_LIMIT:
            normal_failures.append(i)
    topology = not missing and Counter(oriented_triangle(t) for t in identities[indices].reshape(-1, 3)) == Counter(source_faces)
    normal = surface['normal'][:, :3].astype(np.float64)
    tangent = surface['tangent'][:, :3].astype(np.float64)
    nlength, tlength = np.linalg.norm(normal, axis=1), np.linalg.norm(tangent, axis=1)
    cosine = np.abs(np.sum(normal * tangent, axis=1)) / np.maximum(nlength * tlength, 1e-20)
    return dict(passed=not missing and not normal_failures and topology,
                cooked_vertices=len(positions), triangles=len(indices) // 3,
                uv_channels=source['uv_channels'], color_channels=1,
                missing_surface_corner_count=len(missing), missing_surface_corner_examples=missing[:20],
                normal_failure_count=len(normal_failures), normal_failure_examples=normal_failures[:20],
                normal_component_limit=NORMAL_COMPONENT_LIMIT, max_normal_component_error=max_error,
                surface_triangle_multiplicity_and_winding_equal=bool(topology),
                tangent_observations=dict(normal_length_range=[float(nlength.min()), float(nlength.max())],
                    tangent_length_range=[float(tlength.min()), float(tlength.max())],
                    max_absolute_normal_tangent_cosine=float(cosine.max()),
                    nonorthogonal_over_003_count=int(np.count_nonzero(cosine > 0.03)),
                    nonorthogonal_over_01_count=int(np.count_nonzero(cosine > 0.1)),
                    zero_tangent_count=int(np.count_nonzero(tlength < 0.5))))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'readback', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs = {str(args.source): sha(args.source), str(Path(__file__)): sha(Path(__file__)),
              str(Path(__file__).with_name('check_import_cooked_mesh.py')): sha(Path(__file__).with_name('check_import_cooked_mesh.py'))}
    source = json.loads(args.source.read_text())
    skin = read_audit(args.readback, 'skin', inputs)
    path = args.readback / 'skin-surface.json'
    inputs[str(path)] = sha(path)
    audit = json.loads(path.read_text())
    path = args.readback / audit['file']['name']
    if path.stat().st_size != audit['file']['bytes'] or sha(path) != audit['file']['sha256']:
        raise ValueError('Surface payload changed')
    inputs[str(path)] = audit['file']['sha256']
    dtype = np.dtype([('normal', '<f4', (4,)), ('tangent', '<f4', (4,)),
                      ('uv', '<f4', (audit['uv_channels'], 2)),
                      ('color', 'u1', (len(audit['color_channels']), 4))])
    surface = np.fromfile(path, dtype=dtype)
    position_dtype = np.dtype(dict(names=['position'], formats=[('<f4', (3,))], offsets=[0], itemsize=skin['vertex_stride']))
    positions = np.fromfile(args.readback / 'skin-vertices.bin', dtype=position_dtype)['position']
    indices = np.fromfile(args.readback / 'skin-indices.bin', dtype='<u4')
    if (dtype.itemsize != audit['stride'] or len(surface) != audit['vertices'] or
            len(surface) != len(positions) or len(positions) != skin['vertex_count'] or
            len(indices) != skin['index_count'] or len(indices) % 3 or np.any(indices >= len(positions))):
        raise ValueError('Surface and geometry readback layouts disagree')
    report = compare(source, positions, surface, indices, skin['sections'])
    if any(sha(Path(p)) != h for p, h in inputs.items()):
        raise ValueError('Input changed during comparison')
    report.update(asset=skin['asset'], inputs=inputs,
                  scope='LOD0 float32 position/material/all-UV/color corner and triangle correspondence, bounded imported-normal correspondence. Tangent metrics are observations, not source reproduction or shading/gameplay acceptance.')
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'inputs'}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

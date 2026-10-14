"""Locate cooked tangent exceptions using the established Peony zero-UV check.

Python 3.14 with NumPy. Records unresolved fallback frames without treating them
as correct shading. Does not modify geometry, UVs or packed directions.
"""
import argparse
import json
from pathlib import Path

import numpy as np

from check_import_cooked_mesh import read_audit, sha
from measure_cooked_tangent_basis import signed_bytes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--readback', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs = {str(Path(__file__)): sha(Path(__file__))}
    for name in ('check_import_cooked_mesh.py', 'measure_cooked_tangent_basis.py'):
        path = Path(__file__).with_name(name)
        inputs[str(path)] = sha(path)
    audit = read_audit(args.readback, 'skin', inputs)
    path = args.readback / 'source-mesh.json'
    inputs[str(path)] = sha(path)
    mesh = json.loads(path.read_text())
    if mesh['Properties']['LODInfo'][0]['BuildSettings']['bUseHighPrecisionTangentBasis']:
        raise ValueError('This check requires signed-byte tangent packing')
    path = args.readback / 'skin-surface.json'
    inputs[str(path)] = sha(path)
    surface = json.loads(path.read_text())
    path = args.readback / surface['file']['name']
    if sha(path) != surface['file']['sha256'] or path.stat().st_size != surface['file']['bytes']:
        raise ValueError('Surface readback changed')
    inputs[str(path)] = sha(path)
    dtype = np.dtype([('n', '<f4', (4,)), ('t', '<f4', (4,)),
                      ('uv', '<f4', (surface['uv_channels'], 2)),
                      ('color', 'u1', (len(surface['color_channels']), 4))])
    data = np.fromfile(path, dtype=dtype)
    if (dtype.itemsize != surface['stride'] or len(data) != surface['vertices'] or
            len(data) != audit['vertex_count'] or surface['uv_channels'] < 1):
        raise ValueError('Inconsistent surface layout')
    normals = signed_bytes(data['n'][:, :3].astype(float))
    tangents = signed_bytes(data['t'][:, :3].astype(float))
    uv = data['uv'][:, 0].astype(float)
    if not all(np.isfinite(value).all() for value in (normals, tangents, uv)):
        raise ValueError('Nonfinite tangent or UV data')
    indices = np.fromfile(args.readback / 'skin-indices.bin', '<u4').reshape(-1, 3)
    if not len(indices) or int(indices.max()) >= len(data):
        raise ValueError('Invalid triangle indices')
    a, b = uv[indices[:, 1]] - uv[indices[:, 0]], uv[indices[:, 2]] - uv[indices[:, 0]]
    area = np.abs(a[:, 0]*b[:, 1] - a[:, 1]*b[:, 0])
    max_area, incident_count = np.zeros(len(data)), np.zeros(len(data), dtype=np.int64)
    np.maximum.at(max_area, indices.ravel(), np.repeat(area, 3))
    np.add.at(incident_count, indices.ravel(), 1)
    dot = np.abs(np.sum(normals*tangents, axis=1))
    bad = dot > 4/127
    fallback = bad & (incident_count > 0) & (max_area == 0) & np.all(tangents == [1, 0, 0], axis=1)
    unexpected = bad & ~fallback
    unit_error = max(float(np.abs(np.linalg.norm(value, axis=1)-1).max())
                     for value in (normals, tangents))
    unit_bound = float(np.sqrt(3)/127 + 2e-5)
    sections = []
    for section in audit['sections']:
        begin, count = section['FirstIndex'], section['NumFaces']
        if begin % 3 or begin < 0 or count < 0 or begin//3+count > len(indices):
            raise ValueError('Invalid section range')
        mask = np.unique(indices[begin//3:begin//3+count])
        if np.any(bad[mask]):
            material = section['MaterialIndex']
            sections.append(dict(material_index=material, material_slot=audit['material_slots'][material],
                exceptions=int(bad[mask].sum()), fallback_vertices=int(fallback[mask].sum()),
                unexpected_vertices=int(unexpected[mask].sum())))
    changed = [p for p, digest in inputs.items() if sha(Path(p)) != digest]
    if changed:
        raise ValueError('Inputs changed during classification: '+str(changed))
    result = dict(asset=audit['asset'], vertices=len(data), triangles=len(indices),
        maximum_unit_length_error=unit_error, unit_length_bound=unit_bound,
        maximum_absolute_dot=float(dot.max()), dot_bound=4/127,
        nonorthogonal_vertices=int(bad.sum()), zero_uv_unit_x_fallback_vertices=int(fallback.sum()),
        unexpected_nonorthogonal_vertices=int(unexpected.sum()),
        unexpected_vertex_examples=np.flatnonzero(unexpected)[:20].tolist(),
        all_frames_orthogonal=not bool(bad.any()),
        unit_lengths_within_bound=unit_error <= unit_bound,
        only_known_fallback_exceptions=not bool(unexpected.any()),
        sections=sections, inputs=inputs,
        scope='Packed frame lengths and orthogonality classification. Unit-X fallback on collapsed UV0 remains unresolved shading, not visual acceptance. No handedness or normal-map correctness claim.')
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('inputs', 'sections')}))


if __name__ == '__main__':
    main()

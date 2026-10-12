"""Measure saved cloth mapping's rest position and basis endpoint errors in cm."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mapping', type=Path, required=True)
    parser.add_argument('--proxy', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    mapping = json.loads(args.mapping.read_text())
    proxy = json.loads(args.proxy.read_text())['slots']['Slipstream_skirt_Fabric']
    points = np.asarray(proxy['positions'])
    faces = np.asarray(proxy['indices']).reshape(-1, 3)
    face_normals = np.cross(points[faces[:, 1]] - points[faces[:, 0]],
                            points[faces[:, 2]] - points[faces[:, 0]])
    lengths = np.linalg.norm(face_normals, axis=1)
    assert (lengths > 1e-8).all()
    face_normals /= lengths[:, None]
    normals = np.zeros_like(points)
    for column in range(3):
        np.add.at(normals, faces[:, column], face_normals)
    lengths = np.linalg.norm(normals, axis=1)
    assert (lengths > 1e-8).all()
    normals /= lengths[:, None]
    section = next(s for s, meta in zip(mapping['render_geometry']['sections'], mapping['sections'], strict=True)
                   if meta['slot'] == 'Slipstream_skirt_Fabric')
    rows = np.asarray(section['mapping'])
    rest = np.asarray(section['positions'])
    assert rows.shape == (len(rest), 9), 'This checker covers single-influence mapping only'
    active = rows[:, 3] < 65535
    ids = rows[active, :3].astype(int)
    assert np.array_equal(ids, rows[active, :3]) and (ids >= 0).all() and (ids < len(points)).all()
    valid_faces = {tuple(sorted(f)) for f in faces.tolist()}
    assert all(tuple(sorted(f)) in valid_faces for f in ids.tolist())
    result = {}
    targets = (rest, rest + np.asarray(section['normals']), rest + np.asarray(section['tangents']))
    coefficients = (rows[:, 4:8], np.asarray(section['normal_mappings']), np.asarray(section['tangent_mappings']))
    for name, values, target in zip(('position', 'normal_endpoint', 'tangent_endpoint'), coefficients, targets, strict=True):
        values = values[active]
        bary = values[:, :3].copy()
        if name == 'position':
            bary[:, 2] = 1 - values[:, 0] - values[:, 1]
        actual = np.sum(bary[:, :, None] * (points[ids] + normals[ids] * values[:, 3, None, None]), axis=1)
        errors = np.linalg.norm(actual - target[active], axis=1)
        assert np.isfinite(errors).all()
        worst = np.argsort(errors)[-8:][::-1]
        result[name] = dict(max_cm=float(errors.max()), p99_cm=float(np.percentile(errors, 99)),
            over_1mm=int((errors > .1).sum()),
            worst=[dict(vertex=int(np.flatnonzero(active)[i]), error_cm=float(errors[i])) for i in worst])
    report = dict(scope=__doc__, mapping_sha256=digest(args.mapping), proxy_sha256=digest(args.proxy),
        recipe_sha256=digest(Path(__file__)), active_rows=int(active.sum()), endpoints=result,
        limitation='Raw active mapping before skin blending. No moving-pose or native rendering acceptance.')
    with args.output.open('x') as stream:
        stream.write(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

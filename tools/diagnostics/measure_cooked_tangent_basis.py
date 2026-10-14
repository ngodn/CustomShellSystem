"""Measure tangent/normal alignment from verified MeshExport surface streams.

Reports both the reader's legacy UNORM values and the signed-byte interpretation
used by the existing Peony audit. These are diagnostics, not rendered acceptance.
"""
import argparse
import json
from pathlib import Path

import numpy as np

from check_import_cooked_mesh import sha


def metrics(normal, tangent):
    nlength, tlength = np.linalg.norm(normal, axis=1), np.linalg.norm(tangent, axis=1)
    cosine = np.abs(np.sum(normal*tangent, axis=1)) / np.maximum(nlength*tlength, 1e-20)
    return dict(nonorthogonal_over_01=int(np.count_nonzero(cosine > .1)),
                nonorthogonal_over_003=int(np.count_nonzero(cosine > .03)),
                zero_tangents=int(np.count_nonzero(tlength < .5)), max_cosine=float(cosine.max()))


def signed_bytes(values):
    biased = np.rint((values+1)*127.5)
    if not np.allclose(biased/127.5-1, values, atol=2e-7, rtol=0):
        raise ValueError('Directions are not on the expected packed-byte grid')
    return np.maximum((biased-128)/127, -1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--readback', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs = {str(Path(__file__)): sha(Path(__file__))}
    rows = []
    for directory in args.readback:
        path = directory / 'skin-surface.json'
        inputs[str(path)] = sha(path)
        audit = json.loads(path.read_text())
        path = directory / audit['file']['name']
        if sha(path) != audit['file']['sha256'] or path.stat().st_size != audit['file']['bytes']:
            raise ValueError('Readback changed')
        inputs[str(path)] = audit['file']['sha256']
        dtype = np.dtype([('n', '<f4', (4,)), ('t', '<f4', (4,)),
                          ('uv', '<f4', (audit['uv_channels'], 2)),
                          ('color', 'u1', (len(audit['color_channels']), 4))])
        data = np.fromfile(path, dtype=dtype)
        if dtype.itemsize != audit['stride'] or len(data) != audit['vertices']:
            raise ValueError('Invalid readback layout')
        normal, tangent = data['n'][:, :3].astype(float), data['t'][:, :3].astype(float)
        if not np.isfinite(normal).all() or not np.isfinite(tangent).all():
            raise ValueError('Nonfinite tangent basis')
        rows.append(dict(name=directory.parent.name, vertices=len(data),
            reader_values=metrics(normal, tangent),
            signed_byte_interpretation=metrics(signed_bytes(normal), signed_bytes(tangent))))
    if any(sha(Path(p)) != h for p, h in inputs.items()):
        raise ValueError('Input changed')
    args.output.write_text(json.dumps(dict(rows=rows, inputs=inputs,
        scope='Tangent basis observations in both decode interpretations. Does not establish rendered symptoms, performance or source tangent reproduction.'), indent=2)+'\n')
    print(json.dumps(rows))


if __name__ == '__main__':
    main()

"""Measure an audited material surface from captured post-process bone transforms."""
import argparse
import json
from pathlib import Path

import numpy as np

from cooked_contact_pose import sha
from reparent_source_legs import rotate, transforms


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('skin', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('reports', type=Path, nargs='+')
    parser.add_argument('--material', type=int, required=True)
    parser.add_argument('--per-foot', action='store_true')
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs = {}

    def read(path):
        inputs[str(path.resolve())] = sha(path)
        return json.loads(path.read_text())

    skin = read(args.skin / 'skin-audit.json')
    if skin['schema'] != 1:
        raise ValueError('Unsupported skin format')
    for file in skin['files']:
        path = args.skin / file['name']
        if sha(path) != file['sha256'] or path.stat().st_size != file['bytes']:
            raise ValueError('Audited skin changed')
        inputs[str(path.resolve())] = file['sha256']
    dtype = np.dtype([('position', '<f4', 3), ('influences', [
        ('bone', '<u2'), ('raw', '<u2'), ('weight', '<f4')], skin['max_influences'])])
    if dtype.itemsize != skin['vertex_stride']:
        raise ValueError('Unexpected skin stride')
    vertices = np.fromfile(args.skin / 'skin-vertices.bin', dtype=dtype)
    indices = np.fromfile(args.skin / 'skin-indices.bin', dtype='<u4')
    sections = [s for s in skin['sections'] if s['MaterialIndex'] == args.material]
    if (not sections or len(vertices) != skin['vertex_count'] or
            len(indices) != skin['index_count'] or np.any(indices >= len(vertices))):
        raise ValueError('Missing surface or incomplete skin')
    ids = np.unique(np.concatenate([indices[s['FirstIndex']:s['FirstIndex'] + s['NumFaces'] * 3]
                                    for s in sections]))
    selected = np.unique(vertices[ids])
    inf = selected['influences']
    bones = skin['bones']
    bind = transforms(bones)
    parts = []
    foot_rows = {}
    if args.per_foot:
        for side in ('l', 'r'):
            allowed = [i for i, b in enumerate(bones) if b['name'] in (f'calf_{side}', f'foot_{side}')]
            weights = np.sum(np.where(np.isin(inf['bone'], allowed), inf['weight'], 0), axis=1)
            foot_rows[side] = np.flatnonzero(weights >= .9999)
        if (not all(len(rows) for rows in foot_rows.values()) or
                len(np.unique(np.concatenate(list(foot_rows.values())))) != len(selected) or
                len(foot_rows['l']) + len(foot_rows['r']) != len(selected)):
            raise ValueError('Per-foot measurement requires separate calf/foot-weighted surfaces')
    reconstructed = np.zeros((len(selected), 3))
    for bone in np.unique(inf['bone'][inf['weight'] > 0]):
        rows, slots = np.where((inf['bone'] == bone) & (inf['weight'] > 0))
        if len(np.unique(rows)) != len(rows):
            raise ValueError('Duplicate bone influence')
        weight = inf['weight'][rows, slots].astype(float)
        p, q, s = (x[0] for x in bind[bone])
        inverse = q.copy()
        inverse[:3] *= -1
        local = rotate(inverse, selected['position'][rows] - p) / s
        reconstructed[rows] += (p + rotate(q, s * local)) * weight[:, None]
        parts.append((bones[bone]['name'], rows, local, weight))
    bind_error = float(np.max(np.abs(reconstructed - selected['position'])))
    if bind_error > .001:
        raise ValueError('Skin reconstruction failed')
    results = []
    for path in args.reports:
        report = read(path)
        if not report.get('complete') or not report.get('passed'):
            raise ValueError('Incomplete or failed adapter evaluation')
        for result in report['results']:
            if result['mesh'].split('.')[0] != skin['asset'].split('.')[0]:
                raise ValueError('Captured mesh differs from audited skin')
            if result.get('contact_space') != 'component; qx,qy,qz,qw,tx,ty,tz,sx,sy,sz; identity placement':
                raise ValueError('Unexpected transform space or layout')
            names = result['contact_bones']
            frames = result['contact_frames']
            expected = [c['frames'] for c in result['cases'] if c['case'] == 'full_clip']
            if not frames or expected != [len(frames)] or len(set(names)) != len(names):
                raise ValueError('Incomplete or ambiguous capture')
            times = np.array([f['time'] for f in frames])
            values = np.array([f['transforms'] for f in frames])
            if (values.shape != (len(frames), len(names), 10) or not np.isfinite(values).all() or
                    not np.isfinite(times).all() or times[0] != 0 or np.any(np.diff(times) <= 0) or
                    np.any(values[:, :, 7:] <= 0) or
                    np.max(np.abs(np.linalg.norm(values[:, :, :4], axis=2) - 1)) > .001):
                raise ValueError('Invalid evaluated transforms or clock')
            minima = []
            per_foot = {side: [] for side in foot_rows}
            for start in range(0, len(frames), 64):
                batch = values[start:start + 64]
                z = np.zeros((len(batch), len(selected)))
                for name, rows, local, weight in parts:
                    t = batch[:, names.index(name)]
                    x, y, qz, w = t[:, :4].T
                    axis = np.stack((2 * (x * qz - w * y), 2 * (y * qz + w * x),
                                     1 - 2 * (x * x + y * y)), axis=1)
                    z[:, rows] += ((axis * t[:, 7:]) @ local.T + t[:, 6, None]) * weight
                minima.extend(z.min(axis=1).tolist())
                for side, rows in foot_rows.items():
                    per_foot[side].extend(z[:, rows].min(axis=1).tolist())
            worst = int(np.argmin(minima))
            results.append(dict(id=result['id'], mesh=result['mesh'], frames=len(frames),
                                min_z_cm=minima[worst], worst_time_seconds=float(times[worst]),
                                times=times.tolist(), frame_min_z_cm=minima,
                                per_foot_min_z_cm=per_foot))
    if not results or any(sha(Path(path)) != value for path, value in inputs.items()):
        raise ValueError('Missing results or changed inputs')
    args.output.write_text(json.dumps(dict(
        complete=True, inputs=inputs, material=args.material, vertices=len(ids),
        unique_vertices=len(selected), bind_error_cm=bind_error, results=results,
        scope='Final post-process component pose in an identity preview scene. No gameplay floor, '
              'mesh offset, customization or cloth simulation. Diagnostic, not live acceptance.'
    ), indent=2) + '\n')
    print([(r['id'], r['min_z_cm'], r['worst_time_seconds']) for r in results])


if __name__ == '__main__':
    main()

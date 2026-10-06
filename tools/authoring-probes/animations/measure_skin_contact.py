"""Measure neutral cooked lower legs or explicit material surfaces against z=0."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

import numpy as np

from reparent_source_legs import rotate, transforms
from cooked_contact_pose import load_cooked


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('skin', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('motions', type=Path, nargs='+')
    parser.add_argument('--cooked-skeleton', type=Path, help='Audited Skeleton readback for cooked binary motions')
    parser.add_argument('--materials', type=int, nargs='+',
                        help='Measure every referenced vertex in these material slots, without the lower-leg filter')
    parser.add_argument('--deduplicate', action='store_true',
                        help='Evaluate identical position/influence records once, retaining every section membership')
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    manifest = json.loads((args.skin / 'skin-audit.json').read_text())
    if manifest['schema'] != 1:
        raise ValueError('Unsupported skin audit schema')
    inputs = {str((args.skin / 'skin-audit.json').resolve()): sha(args.skin / 'skin-audit.json')}
    for row in manifest['files']:
        path = args.skin / row['name']
        if path.stat().st_size != row['bytes'] or sha(path) != row['sha256']:
            raise ValueError('Skin audit data changed')
        inputs[str(path.resolve())] = row['sha256']
    dtype = np.dtype([('position', '<f4', 3), ('influences', [
        ('bone', '<u2'), ('raw', '<u2'), ('weight', '<f4')], manifest['max_influences'])])
    if dtype.itemsize != manifest['vertex_stride']:
        raise ValueError('Unexpected skin vertex stride')
    vertices = np.fromfile(args.skin / 'skin-vertices.bin', dtype=dtype)
    indices = np.fromfile(args.skin / 'skin-indices.bin', dtype='<u4')
    if len(vertices) != manifest['vertex_count'] or len(indices) != manifest['index_count']:
        raise ValueError('Wrong vertex or index count')
    used = np.zeros(len(vertices), dtype=bool)
    used[indices] = True
    names = [b['name'] for b in manifest['bones']]
    bind = transforms(manifest['bones'])
    selected = {}
    influences = vertices['influences']
    if args.materials is not None:
        materials = set(args.materials)
        available = {s['MaterialIndex'] for s in manifest['sections']}
        if len(materials) != len(args.materials) or not materials <= available:
            raise ValueError('Duplicate or absent material section')
        selected['surface'] = np.unique(np.concatenate([
            indices[s['FirstIndex']:s['FirstIndex'] + s['NumFaces'] * 3]
            for s in manifest['sections'] if s['MaterialIndex'] in materials]))
        if not len(selected['surface']):
            raise ValueError('No referenced surface vertices')
    else:
        for side in ('l', 'r'):
            lower_leg = [names.index(name + '_' + side) for name in ('calf', 'foot', 'ball')]
            weight = np.sum(np.where(np.isin(influences['bone'], lower_leg), influences['weight'], 0), axis=1)
            selected[side] = np.flatnonzero(used & (weight >= .5) & (vertices['position'][:, 2] <= 30))
            if not len(selected[side]):
                raise ValueError('No lower-leg surface vertices')
        if len(np.intersect1d(*selected.values())):
            raise ValueError('Ambiguous left/right lower-leg vertices')
    prepared = {}
    for side, ids in selected.items():
        all_ids = ids
        if args.deduplicate:
            subset, first, remap = np.unique(vertices[ids], return_index=True, return_inverse=True)
            ids = ids[first]
        else:
            subset, remap = vertices[ids], np.arange(len(ids))
        inf = subset['influences']
        components = []
        reconstructed = np.zeros((len(ids), 3))
        for bone in np.unique(inf['bone'][inf['weight'] > 0]):
            row, slot = np.where((inf['bone'] == bone) & (inf['weight'] > 0))
            if len(np.unique(row)) != len(row):
                raise ValueError('Duplicate bone influence')
            weight = inf['weight'][row, slot].astype(float)
            bp, bq, bs = (x[0] for x in bind[bone])
            inverse = bq.copy(); inverse[:3] *= -1
            local = rotate(inverse, subset['position'][row] - bp) / bs
            reconstructed[row] += (bp + rotate(bq, local * bs)) * weight[:, None]
            components.append((int(bone), row, local, weight))
        error = float(np.max(np.abs(reconstructed - subset['position'])))
        if error > 1e-4:
            raise ValueError(f'Bind reconstruction error: {error} cm')
        sections = {}
        for section in manifest['sections']:
            if args.materials is not None and section['MaterialIndex'] not in materials:
                continue
            start = section['FirstIndex']
            used_section = np.unique(indices[start:start + section['NumFaces'] * 3])
            rows = np.unique(remap[np.isin(all_ids, used_section)])
            if len(rows):
                key = str(section['MaterialIndex'])
                sections[key] = np.union1d(sections.get(key, np.array([], dtype=int)), rows)
        prepared[side] = (ids, components, sections, error, len(all_ids))
    results = []
    for motion in args.motions:
        inputs[str(motion.resolve())] = sha(motion)
        if args.cooked_skeleton:
            world, data = load_cooked(motion, manifest['bones'], args.cooked_skeleton, inputs)
            frame_count = data['frames']
        else:
            world, data, frame_count = load_raw(motion, names, manifest)
        result = dict(motion=str(motion), frames=frame_count, fps=data['fps'],
                      sampling=data.get('sampling', 'Unspecified by capture'), sides={})
        for side, (ids, components, sections, error, original_count) in prepared.items():
            minima = np.full(frame_count, np.inf)
            worst_ids = np.zeros(frame_count, dtype=int)
            section_minima = {key: np.full(frame_count, np.inf) for key in sections}
            for start in range(0, frame_count, 64):
                end = min(start + 64, frame_count)
                z = np.zeros((end - start, len(ids)))
                for bone, rows, local, weight in components:
                    p, q, scale = (x[start:end] for x in world[bone])
                    x, y, qz, w = q.T
                    axis = np.stack((2 * (x * qz - w * y), 2 * (y * qz + w * x),
                                     1 - 2 * (x * x + y * y)), axis=1)
                    z[:, rows] += ((axis * scale) @ local.T + p[:, 2, None]) * weight
                if not np.isfinite(z).all():
                    raise ValueError('Non-finite skinned position')
                worst = np.argmin(z, axis=1)
                minima[start:end] = z[np.arange(end - start), worst]
                worst_ids[start:end] = ids[worst]
                for key, rows in sections.items():
                    section_minima[key][start:end] = z[:, rows].min(axis=1)
            worst = int(np.argmin(minima))
            result['sides'][side] = dict(vertices=original_count, evaluated_vertices=len(ids), bind_error_cm=error,
                min_z_cm=float(minima[worst]), worst_frame=worst,
                worst_time_seconds=worst / data['fps'], worst_vertex=int(worst_ids[worst]),
                frames_below_minus_0_1_cm=int(np.count_nonzero(minima < -.1)),
                material_min_z_cm={key: float(values.min()) for key, values in section_minima.items()},
                material_frame_min_z_cm={key: values.tolist() for key, values in section_minima.items()},
                frame_min_z_cm=minima.tolist())
        results.append(result)
        print(motion.name, {s: r['min_z_cm'] for s, r in result['sides'].items()}, flush=True)
    if any(sha(Path(path)) != checksum for path, checksum in inputs.items()):
        raise ValueError('Input changed during measurement')
    selection = ('Every referenced vertex in the explicit material sections, with all influences and no height or limb filter.'
                 if args.materials is not None else
                 'Referenced vertices at bind Z <= 30 cm with at least 50% combined calf/foot/ball weight on one side. All their influences preserved; all material sections included.')
    args.output.write_text(json.dumps(dict(complete=True, inputs=inputs, results=results,
        selection=selection, materials=args.materials,
        scope='Neutral cooked surface linear skinning at every supplied pose, floor Z=0 cm. Sampling is determined by the input capture. No morphs, post-process, physics, cloth simulation or material visibility filtering. Diagnostic, not full contact or live acceptance.'
    ), indent=2) + '\n')


def load_raw(motion, names, manifest):
    opener = gzip.open if motion.suffix == '.gz' else open
    with opener(motion, 'rt') as stream:
        data = json.load(stream)
    snapshots = [f['pose']['Snapshot'] for f in data['frames']]
    order = snapshots[0]['BoneNames']
    if any(not s['bIsValid'] or s['BoneNames'] != order for s in snapshots):
        raise ValueError('Invalid or inconsistent snapshots')
    tracks = []
    for name in names:
        index = order.index(name)
        keys = []
        for snapshot in snapshots:
            t = snapshot['LocalTransforms'][index]
            keys.append({field: [t[unreal][c] for c in axes] for field, unreal, axes in (
                ('translation', 'Translation', 'XYZ'), ('rotation', 'Rotation', 'XYZW'),
                ('scale', 'Scale3D', 'XYZ'))})
        tracks.append(dict(name=name, keys=keys))
    world = transforms(manifest['bones'], tracks, len(snapshots))
    return world, data, len(snapshots)


if __name__ == '__main__':
    main()

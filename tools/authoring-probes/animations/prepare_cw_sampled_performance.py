"""Fit a sampled CW performance without changing the existing expression shapes."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from cw_facial_basis import compact_basis, filtered_shapes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--holdouts', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--retain-performance-controls', action='store_true')
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    protected = {}

    def protect(path):
        path = path.resolve(strict=True)
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        if protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Input changed: ' + str(path))
        return path

    def read(path):
        return json.loads(protect(path).read_text())

    work = args.work.resolve(strict=True)
    fit = read(work/'cw-retained-basis-v1/report.json')
    bake = read(work/'cw-expression-bake-v1/report.json')
    transfer = read(work/'cw-performance-fitted-v1/report.json')
    plan = read(args.source/'plan.json')
    if not all((fit.get('complete'), bake.get('sources_unchanged'), transfer.get('complete'), plan.get('prepared'))):
        raise ValueError('Require verified retained shapes and source plan')
    for audit in (transfer, plan):
        for path, checksum in audit['protected'].items():
            if protected[str(protect(Path(path)))] != checksum:
                raise ValueError('Audited input no longer matches: ' + path)
    for folder in (args.source, args.holdouts):
        audit = read(folder/'samples/report.json')
        if (read(folder/'exit.json')['exit_code'] != 0 or not audit.get('sources_unchanged') or
                not audit.get('eye_gaze_range') or audit.get('tongue_fk')):
            raise ValueError('Source sampling or restoration failed')
        if protected[str(protect(Path(audit['source'])))] != audit['sha256']:
            raise ValueError('Original CW source changed')
    parts = fit['parts']

    def points(folder, label):
        with np.load(protect(folder/'samples'/(label+'.npz'))) as data:
            return np.concatenate([data[name] for name in parts])

    neutral = points(args.source, 'neutral')
    if np.max(np.abs(neutral-points(args.holdouts, 'neutral'))) > 1e-6:
        raise ValueError('Holdout neutral differs')
    ids = []
    for part in parts:
        offset = bake['offsets'][part]
        if offset >= bake['offsets']['Commander White Top Mesh']:
            offset += bake['topology_insertion_points']
        ids.extend(range(offset, offset+fit['part_counts'][part]))
    if len(ids) != len(neutral) or len(set(ids)) != len(ids):
        raise ValueError('Source-to-fitted topology mapping differs')
    index = {point: i for i, point in enumerate(ids)}
    old = read(work/'cw-expression-compact-v2/facial-shapes.json')
    payload = read(work/'cw-performance-fitted-v1/facial-shapes.json')
    names = list(fit['existing_names'])
    if args.retain_performance_controls:
        names += [row['name'] for row in payload['morph_targets']]
    existing = np.zeros((len(names), len(neutral), 3))
    for source in (old, payload):
        for row in source['morph_targets']:
            if row['name'] not in names:
                continue
            shape = existing[names.index(row['name'])]
            for point, x, y, z in row['deltas']:
                source_index = index.get(source['point_indices'][point])
                if source_index is not None:
                    shape[source_index] = [x, -y, z]
    existing = existing.astype(np.float32).astype(float)
    existing[np.linalg.norm(existing, axis=2) <= .015] = 0
    samples = np.stack([points(args.source, row['label'])-neutral for row in plan['samples']])
    active = np.flatnonzero(np.any(np.abs(samples) > 1e-9, axis=(0, 2)) |
                            np.any(np.abs(existing) > 1e-9, axis=(0, 2)))
    verified_points = dict(zip(payload['point_indices'], payload['body_points'], strict=True))
    correspondence = 0.0
    for point in active:
        if ids[point] not in verified_points:
            raise ValueError('Performance moves a point outside the verified fitted transfer')
        correspondence = max(correspondence, float(np.linalg.norm(
            neutral[point]*[1, -1, 1]-verified_points[ids[point]])))
    if correspondence > .0002:
        raise ValueError('Performance neutral differs from the verified fitted transfer')
    outside = np.ones(len(neutral), dtype=bool)
    outside[active] = False
    samples = samples[:, active]
    previous = existing[:, active].reshape(len(names), -1)
    old_weights, _, rank, _ = np.linalg.lstsq(previous.T, samples.reshape(len(samples), -1).T, rcond=.001)
    old_weights = old_weights.T
    if np.abs(old_weights).max() > 4:
        raise ValueError('Existing-shape projection exceeds the runtime coefficient bound')
    residual = samples-(old_weights@previous).reshape(samples.shape)
    basis, weights, error = compact_basis(residual, .001, 128-len(names))
    _, gains = filtered_shapes(basis, .015, 32)
    stored = (basis*gains[:, None, None]).astype(np.float32)
    if np.linalg.norm(stored, axis=2).max() > 20:
        raise ValueError('Performance shape exceeds the unchanged native delta bound')
    filtered = np.where((np.linalg.norm(stored, axis=2) > .015)[:, :, None], stored, 0)
    coefficients = np.concatenate((old_weights, weights/gains), axis=1).astype(np.float32).astype(float)
    shapes = np.concatenate((existing[:, active], filtered))
    actual = (coefficients@shapes.reshape(len(shapes), -1)).reshape(samples.shape)
    training_error = float(np.linalg.norm(actual-samples, axis=2).max())
    if training_error > .015 or np.abs(coefficients).max() > 4:
        raise ValueError('Filtered performance exceeds the existing geometry or weight bound')
    times = np.asarray([row['seconds'] for row in plan['samples']])
    cases = []
    for row in plan['holdouts']:
        seconds = row['seconds']
        values = np.array([np.interp(seconds, times, coefficients[:, i]) for i in range(len(shapes))])
        expected = points(args.holdouts, row['label'])-neutral
        predicted = (values@shapes.reshape(len(shapes), -1)).reshape(-1, 3)
        maximum = float(np.linalg.norm(expected[active]-predicted, axis=1).max())
        if outside.any():
            maximum = max(maximum, float(np.linalg.norm(expected[outside], axis=1).max()))
        cases.append(dict(seconds=seconds, error_cm=maximum))
    maximum = max(row['error_cm'] for row in cases)
    passed = maximum <= .015
    new_names = [f'CSSF_CW_Spin_{i:03}' for i in range(len(basis))]
    fitted_shapes = []
    for name, shape in zip(new_names, stored*[1, -1, 1], strict=True):
        selected = np.flatnonzero(np.any(shape != 0, axis=1))
        fitted_shapes.append(dict(name=name, deltas=[[int(i), *shape[i].tolist()] for i in selected]))
    fitted = dict(schema=2, point_indices=np.asarray(ids)[active].tolist(),
                  body_points=(neutral[active]*[1, -1, 1]).tolist(), morph_targets=fitted_shapes)
    encoded = json.dumps(fitted, separators=(',', ':'), allow_nan=False)+'\n'
    if len(encoded.encode()) > 64*1024*1024:
        raise ValueError('Fitted payload exceeds the unchanged native input bound')
    for path in list(protected):
        protect(Path(path))
    args.output.mkdir(parents=True)
    np.savez_compressed(args.output/'basis.npz', names=np.asarray(names+new_names), active=active,
                        times=times, coefficients=coefficients, shapes=shapes)
    output = args.output/'facial-shapes.json'
    output.write_text(encoded)
    report = dict(passed=passed, protected=protected, clip=plan['clip'], added_shapes=len(basis),
                  source_samples=len(samples), catalog_shapes=len(shapes), existing_projection_rank=int(rank),
                  fitted_correspondence_error_cm=correspondence,
                  basis_error_cm=error, training_error_cm=training_error, holdouts=cases,
                  max_holdout_error_cm=maximum, limit_cm=.015,
                  sha256=hashlib.sha256(output.read_bytes()).hexdigest(), installed=False,
                  scope='Source-rig geometry and independent temporal holdouts. Fitted import, cooked readback and live acceptance remain pending.')
    (args.output/'report.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print('CW_SAMPLED_PERFORMANCE', passed, len(basis), training_error, maximum, flush=True)
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()

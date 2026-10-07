"""Reduce each facial curve separately, then verify the result against evaluated source poses."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from compile_sampled_face import geometry_errors, interpolate


def reduce_keys(times, values, tolerance):
    keep = {0, len(times)-1}
    pending = [(0, len(times)-1)]
    while pending:
        start, end = pending.pop()
        if end-start <= 1:
            continue
        fraction = (times[start+1:end]-times[start])/(times[end]-times[start])
        errors = np.abs(values[start+1:end]-(values[start]+fraction*(values[end]-values[start])))
        if errors.max() > tolerance:
            split = start+1+int(errors.argmax())
            keep.add(split)
            pending.extend(((start, split), (split, end)))
    return np.asarray(sorted(keep))


def refine_geometry_keys(times, values, shapes, curves, limit_cm=.009):
    keep = [set(np.searchsorted(times, keys[:, 0]).tolist()) for keys in curves]
    active = np.any(values != 0, axis=0)
    steps = []
    for iteration in range(24):
        curves = [np.stack((times[sorted(ids)], values[sorted(ids), i]), axis=1)
                  for i, ids in enumerate(keep)]
        count = sum(len(ids) for ids, used in zip(keep, active, strict=True) if used)
        if count > 65536:
            raise ValueError('Geometry refinement exceeds native key count')
        differences = sample_curves(curves, times, rounded=False)-values
        maximum, insertions = 0.0, set()
        for begin in range(0, len(times), 128):
            delta = differences[begin:begin+128]
            errors = (delta@shapes.reshape(len(shapes), -1)).reshape(len(delta), -1, 3)
            norms = np.linalg.norm(errors, axis=2)
            points = norms.argmax(axis=1)
            worst = norms[np.arange(len(delta)), points]
            maximum = max(maximum, float(worst.max()))
            for row in np.flatnonzero(worst > limit_cm):
                # Restore the curve contributing most to this vertex's error.
                contributions = np.einsum('ij,j->i', shapes[:, points[row]], errors[row, points[row]])*delta[row]
                curve, key = int(contributions.argmax()), begin+int(row)
                if key not in keep[curve]:
                    insertions.add((curve, key))
        steps.append(dict(iteration=iteration, keys=count, maximum_cm=maximum, insertions=len(insertions)))
        if maximum <= limit_cm:
            return curves, steps
        if not insertions or count+len(insertions) > 65536:
            raise ValueError('Cannot refine geometry within native key count')
        for curve, key in insertions:
            keep[curve].add(key)
    raise ValueError('Geometry refinement did not converge within 24 iterations')


def compile_sparse(times, values, shapes, curve_error_cm, *, refine_geometry=False):
    times, values, shapes = (np.asarray(x, dtype=float) for x in (times, values, shapes))
    if (times.ndim != 1 or not 2 <= len(times) <= 16384 or np.any(np.diff(times) <= 0) or
            times[0] < 0 or times[-1] > 600 or values.ndim != 2 or values.shape[0] != len(times) or
            not 1 <= values.shape[1] <= 128 or shapes.ndim != 3 or
            shapes.shape[0] != values.shape[1] or shapes.shape[1] < 1 or shapes.shape[2] != 3 or
            not all(np.isfinite(a).all() for a in (times, values, shapes)) or np.abs(values).max() > 4 or
            not np.isfinite(curve_error_cm) or not 0 < curve_error_cm <= .01):
        raise ValueError('Expected bounded, ordered samples and finite stored morph deltas')
    stored_times = np.unique(times.astype(np.float32)).astype(float)
    if len(stored_times) < 2:
        raise ValueError('Timeline collapses at runtime precision')
    stored_values = interpolate(times, values, stored_times).astype(np.float32).astype(float)
    strengths = np.linalg.norm(shapes, axis=2).max(axis=1)
    curves = []
    for index, strength in enumerate(strengths):
        selected = reduce_keys(stored_times, stored_values[:, index], curve_error_cm/max(strength, 1e-12))
        curves.append(np.stack((stored_times[selected], stored_values[selected, index]), axis=1))
    refinement = []
    if refine_geometry:
        curves, refinement = refine_geometry_keys(stored_times, stored_values, shapes, curves)
    queries = np.unique(np.concatenate((times, stored_times)))
    expected = interpolate(times, values, queries)
    actual = sample_curves(curves, queries, rounded=False)
    bound = float(geometry_errors(actual-expected, shapes).max())
    # Runtime lerp ends in a float cast. Include a conservative rounding term
    # separately from the piecewise-linear endpoint proof.
    rounding = float(np.sum(strengths*np.max(np.abs(stored_values), axis=0))*np.finfo(np.float32).eps)
    if bound+rounding > .01:
        raise ValueError('Independent curve reduction exceeds the conversion budget')
    return curves, dict(source_samples=len(times), runtime_samples=len(stored_times),
                        curve_error_cm=curve_error_cm, conversion_bound_cm=bound,
                        geometry_refinement=refinement,
                        runtime_weight_rounding_bound_cm=rounding, conversion_limit_cm=.01)


def sample_curves(curves, times, *, rounded=True):
    values = np.stack([np.interp(times, keys[:, 0], keys[:, 1]) for keys in curves], axis=1)
    return values.astype(np.float32).astype(float) if rounded else values


def restore_source_intervals(curves, times, values, queries):
    stored_times = np.unique(times.astype(np.float32)).astype(float)
    stored_values = interpolate(times, values, stored_times).astype(np.float32).astype(float)
    after = np.clip(np.searchsorted(stored_times, queries), 0, len(stored_times)-1)
    indices = np.unique(np.concatenate((after, np.maximum(after-1, 0))))
    result = []
    for i, keys in enumerate(curves):
        selected = indices[np.abs(np.interp(stored_times[indices], keys[:, 0], keys[:, 1])-
                                  stored_values[indices, i]) > 1e-12]
        added = np.stack((stored_times[selected], stored_values[selected, i]), axis=1)
        merged = np.concatenate((keys, added))
        result.append(merged[np.argsort(merged[:, 0])])
    check_runtime_limits(result)
    if all(np.array_equal(a, b) for a, b in zip(curves, result, strict=True)):
        raise ValueError('Source interval restoration made no progress')
    return result


def check_runtime_limits(curves):
    active = [keys for keys in curves if np.any(keys[:, 1] != 0)]
    if (not 1 <= len(active) <= 128 or any(not 1 <= len(keys) <= 16384 for keys in active) or
            sum(map(len, active)) > 65536):
        raise ValueError('Compiled facial curves exceed native parser bounds')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--basis', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--holdouts', type=Path, required=True)
    parser.add_argument('--layout', type=Path, required=True)
    parser.add_argument('--curve-error-cm', type=float, required=True)
    parser.add_argument('--refine-geometry', action='store_true',
                        help='Restore keys by combined vertex error within unchanged native limits')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    protected = {}

    def protect(path):
        path = path.resolve(strict=True)
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        if protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Protected source changed: '+str(path))
        return path

    report = json.loads(protect(args.basis/'report.json').read_text())
    if not report.get('passed') or report['limit_cm'] != .015:
        raise ValueError('Require the accepted source fit and its unchanged geometry limit')
    protected.update(report['protected'])
    if any(str(path.resolve(strict=True)) not in report['protected']
           for path in (args.layout, args.source/'plan.json')):
        raise ValueError('Source layout or plan was not checked by the accepted fit')
    layout = json.loads(protect(args.layout).read_text())
    plan = json.loads(protect(args.source/'plan.json').read_text())
    if plan['clip'] != report['clip']:
        raise ValueError('Wrong source performance')
    with np.load(protect(args.basis/'basis.npz')) as data:
        times, values, shapes, active = (data[name].copy() for name in ('times', 'coefficients', 'shapes', 'active'))
        names = data['names'].tolist()
    if (len(set(names)) != len(names) or len(names) != values.shape[1] or
            not np.array_equal(times, [row['seconds'] for row in plan['samples']])):
        raise ValueError('Basis identities or sample times differ')
    curves, conversion = compile_sparse(times, values, shapes, args.curve_error_cm,
                                       refine_geometry=args.refine_geometry)
    check_runtime_limits(curves)

    def points_at(path):
        path = path.resolve(strict=True)
        if str(path) not in report['protected']:
            raise ValueError('Pose was not checked by the accepted source fit')
        with np.load(protect(path)) as data:
            return np.concatenate([data[name] for name in layout['parts']])

    def points(folder, label):
        return points_at(folder/'samples'/(label+'.npz'))

    neutral = (points_at(Path(report['neutral_reference'])) if report.get('neutral_reference')
               else points(args.source, 'neutral'))
    holdout_neutral = (points_at(Path(report['holdout_neutral_reference'])) if report.get('holdout_neutral_reference')
                       else points(args.holdouts, 'neutral'))
    if (active.dtype.kind not in 'iu' or len(set(active.tolist())) != len(active) or
            active.min() < 0 or active.max() >= len(neutral) or len(active) != shapes.shape[1] or
            not np.array_equal(neutral, holdout_neutral)):
        raise ValueError('Source topology or neutral differs')
    outside = np.ones(len(neutral), dtype=bool)
    outside[active] = False
    source_refinements = []
    for attempt in range(5):
        checks, failed = {}, []
        for kind, folder in [('samples', args.source), ('holdouts', args.holdouts)]:
            cases = []
            rows = plan[kind]
            for offset in range(0, len(rows), 32):
                batch = rows[offset:offset+32]
                queries = [row['seconds'] for row in batch]
                weights = sample_curves(curves, queries)
                predicted = (weights@shapes.reshape(len(shapes), -1)).reshape(len(batch), len(active), 3)
                expected = np.stack([points(folder, row['label'])-neutral for row in batch])
                errors = np.linalg.norm(predicted-expected[:, active], axis=2).max(axis=1)
                if outside.any():
                    errors = np.maximum(errors, np.linalg.norm(expected[:, outside], axis=2).max(axis=1))
                cases.extend(dict(seconds=float(t), error_cm=float(e)) for t, e in zip(queries, errors, strict=True))
            maximum = max(row['error_cm'] for row in cases)
            failed.extend(row for row in cases if row['error_cm'] > report['limit_cm'])
            checks[kind] = dict(poses=len(cases), maximum_cm=maximum, cases=cases)
        if not failed:
            break
        if not args.refine_geometry or attempt == 4:
            raise ValueError(f'Reduced curves fail actual source geometry: {failed}')
        source_refinements.append(dict(attempt=attempt, failed=failed))
        curves = restore_source_intervals(curves, times, values, [row['seconds'] for row in failed])
        queries = np.unique(np.concatenate((times, times.astype(np.float32).astype(float))))
        bound = float(geometry_errors(sample_curves(curves, queries, rounded=False)-
                                      interpolate(times, values, queries), shapes).max())
        if bound+conversion['runtime_weight_rounding_bound_cm'] > conversion['conversion_limit_cm']:
            raise ValueError('Source interval restoration exceeds the conversion budget')
        conversion['conversion_bound_cm'] = bound
        print('CW_SOURCE_INTERVALS_RESTORED', attempt, len(failed), flush=True)
    performance = dict(clip=report['clip'], duration=float(times[-1]), curves=[
        dict(morph=name, keys=keys.tolist()) for name, keys in zip(names, curves, strict=True)
        if np.any(keys[:, 1] != 0)])
    encoded = json.dumps(performance, separators=(',', ':'), allow_nan=False)+'\n'
    if len(encoded.encode()) > 16*1024*1024:
        raise ValueError('Performance exceeds the facial resource byte bound')
    for path in list(protected):
        protect(Path(path))
    args.output.mkdir(parents=True)
    (args.output/'performance.json').write_text(encoded)
    result = dict(passed=True, protected=protected, clip=report['clip'], conversion=conversion,
                  curves=len(performance['curves']), keys=sum(len(c['keys']) for c in performance['curves']),
                  bytes=len(encoded.encode()), geometry_limit_cm=report['limit_cm'], source_checks=checks,
                  source_refinements=source_refinements,
                  sha256=hashlib.sha256(encoded.encode()).hexdigest(), installed=False,
                  scope='Stored runtime curves versus every evaluated source and independent intermediate pose. Cooking, full temporal visual comparison and live acceptance remain pending.')
    (args.output/'report.json').write_text(json.dumps(result, indent=2, allow_nan=False)+'\n')
    print('CW_SPARSE_CURVES', result['curves'], result['keys'], result['bytes'],
          {kind: row['maximum_cm'] for kind, row in checks.items()}, flush=True)


if __name__ == '__main__':
    main()

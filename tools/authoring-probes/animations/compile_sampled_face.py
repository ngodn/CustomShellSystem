"""Reduce sampled facial curves with an error bound on the deformed geometry."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def geometry_errors(weights, shapes):
    errors = []
    for start in range(0, len(weights), 32):
        delta = (weights[start:start+32]@shapes.reshape(len(shapes), -1)).reshape(-1, shapes.shape[1], 3)
        errors.extend(np.linalg.norm(delta, axis=2).max(axis=1))
    return np.asarray(errors)


def interpolate(times, values, queries):
    return np.stack([np.interp(queries, times, values[:, i]) for i in range(values.shape[1])], axis=1)


def compile_samples(times, values, shapes, error_cm=.001):
    times, values, shapes = (np.asarray(x, dtype=float) for x in (times, values, shapes))
    if (times.ndim != 1 or not 2 <= len(times) <= 16384 or np.any(np.diff(times) <= 0) or
            times[0] < 0 or times[-1] > 600 or values.ndim != 2 or values.shape[0] != len(times) or
            not 1 <= values.shape[1] <= 128 or shapes.ndim != 3 or
            shapes.shape[0] != values.shape[1] or shapes.shape[1] < 1 or shapes.shape[2] != 3 or
            not all(np.isfinite(a).all() for a in (times, values, shapes)) or np.abs(values).max() > 4 or
            not np.isfinite(error_cm) or not 0 < error_cm <= .01):
        raise ValueError('Expected bounded, ordered samples and finite stored morph deltas')
    stored_times = np.unique(times.astype(np.float32)).astype(float)
    if len(stored_times) < 2:
        raise ValueError('Timeline collapses at runtime precision')
    stored_values = interpolate(times, values, stored_times).astype(np.float32).astype(float)
    rounding = geometry_errors(interpolate(stored_times, stored_values, times)-values, shapes).max()
    if rounding > error_cm/4:
        raise ValueError('Runtime time/weight precision exceeds the geometric budget')
    keep = {0, len(stored_times)-1}
    pending = [(0, len(stored_times)-1)]
    while pending:
        start, end = pending.pop()
        if end-start <= 1:
            continue
        fraction = (stored_times[start+1:end]-stored_times[start])/(stored_times[end]-stored_times[start])
        predicted = stored_values[start] + fraction[:, None]*(stored_values[end]-stored_values[start])
        errors = geometry_errors(predicted-stored_values[start+1:end], shapes)
        if errors.max() > error_cm-rounding:
            split = start+1+int(errors.argmax())
            keep.add(split)
            pending.extend(((start, split), (split, end)))
    selected = sorted(keep)
    reduced_times, reduced_values = stored_times[selected], stored_values[selected]
    # Include both knot sets. Between knots the vector error is affine, so
    # its norm cannot exceed the larger endpoint norm.
    queries = np.unique(np.concatenate((times, stored_times)))
    expected = interpolate(times, values, queries)
    actual = interpolate(reduced_times, reduced_values, queries)
    bound = float(geometry_errors(actual-expected, shapes).max())
    if bound > error_cm:
        raise ValueError('Reduced runtime curves exceed their geometry bound')
    return reduced_times, reduced_values, dict(source_samples=len(times),
        runtime_samples=len(stored_times), output_keys_per_curve=len(selected),
        float32_bound_cm=float(rounding), conversion_bound_cm=bound, limit_cm=error_cm)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--basis', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    report_path, basis_path = args.basis/'report.json', args.basis/'basis.npz'
    before = {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in (report_path, basis_path)}
    report = json.loads(report_path.read_text())
    if not report.get('passed'):
        raise ValueError('Require independent source-pose acceptance')
    with np.load(basis_path) as data:
        names = data['names'].tolist()
        times, values, shapes = (data[name].copy() for name in ('times', 'coefficients', 'shapes'))
    if len(set(names)) != len(names) or len(names) != values.shape[1]:
        raise ValueError('Morph identities do not match the sampled representation')
    times, values, bound = compile_samples(times, values, shapes)
    curves = [dict(morph=name, keys=np.stack((times, values[:, i]), axis=1).tolist())
              for i, name in enumerate(names) if np.any(values[:, i] != 0)]
    performance = dict(clip=report['clip'], duration=float(times[-1]), curves=curves)
    text = json.dumps(performance, separators=(',', ':'), allow_nan=False)+'\n'
    for path, checksum in before.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != checksum:
            raise ValueError('Basis changed during compilation')
    args.output.mkdir(parents=True)
    (args.output/'performance.json').write_text(text)
    evidence = dict(passed=True, protected=before, clip=report['clip'], curves=len(curves),
                    keys=sum(len(curve['keys']) for curve in curves), bytes=len(text.encode()), bound=bound,
                    source_holdout_plus_conversion_cm=report['max_holdout_error_cm']+bound['conversion_bound_cm'],
                    installed=False,
                    scope='Runtime-precision linear curves versus sampled source geometry. Catalog integration, cooked shapes and live acceptance remain pending.')
    (args.output/'report.json').write_text(json.dumps(evidence, indent=2, allow_nan=False)+'\n')
    print('CW_SAMPLED_CURVES', len(curves), bound, len(text.encode()), flush=True)


if __name__ == '__main__':
    main()

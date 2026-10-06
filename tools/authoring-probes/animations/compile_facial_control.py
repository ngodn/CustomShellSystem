"""Bake one sampled facial control into bounded piecewise-linear morph curves."""
import numpy as np

from audit_cw_facial_interpolation import interpolation_weights, spline_curvatures


def compile_control(keys, shapes, gains, error_cm=.001):
    """Return time/weight rows and a geometric error bound for quarter samples.

    Shapes are stored morph deltas in centimetres. Gains undo storage scaling.
    The bound covers conversion of the cubic control model to linear curves,
    not agreement between that model and the original source rig.
    """
    keys = np.asarray(keys, dtype=float)
    shapes = np.asarray(shapes, dtype=float)
    gains = np.asarray(gains, dtype=float)
    if (keys.ndim != 2 or keys.shape[1] != 2 or not 2 <= len(keys) <= 16384 or
            not np.isfinite(keys).all() or np.any(np.diff(keys[:, 0]) <= 0) or
            keys[0, 0] < 0 or np.any((keys[:, 1] < 0) | (keys[:, 1] > 1)) or
            shapes.ndim != 3 or shapes.shape[0] != 4 or shapes.shape[2] != 3 or
            not shapes.size or not np.isfinite(shapes).all() or
            gains.shape != (4,) or not np.isfinite(gains).all() or np.any((gains < 1) | (gains > 32)) or
            not np.isfinite(error_cm) or not 0 < error_cm <= .01):
        raise ValueError('Expected finite ordered control keys and four quarter-strength shapes')
    axis = (0., .25, .5, .75, 1.)
    physical = np.concatenate((np.zeros_like(shapes[:1]), shapes / gains[:, None, None]))
    curvature = np.einsum('ij,jkl->ikl', spline_curvatures(axis), physical)

    def weights(value):
        return np.array([w for _, w in interpolation_weights(axis, value, 'cubic')][1:]) / gains

    def second_derivative(value):
        high = min(int(np.searchsorted(axis, value, side='right')), 4)
        low = high - 1
        fraction = (value-axis[low]) / (axis[high]-axis[low])
        return (1-fraction)*curvature[low] + fraction*curvature[high]

    times, values = [float(keys[0, 0])], [weights(keys[0, 1])]
    bounds = []

    def interval(ta, va, tb, vb, depth=0):
        # Within one spline span, the second derivative is affine. Its norm
        # is bounded by the endpoints; linear interpolation error is M*h²/8.
        second = max(float(np.linalg.norm(second_derivative(v), axis=1).max()) for v in (va, vb))
        bound = second * (vb-va)**2 / 8
        if bound > error_cm/2:
            if depth == 20:
                raise ValueError('Facial interpolation subdivision limit exceeded')
            tm, vm = (ta+tb)/2, (va+vb)/2
            interval(ta, va, tm, vm, depth+1)
            interval(tm, vm, tb, vb, depth+1)
        else:
            if len(times) >= 16384:
                raise ValueError('Facial interpolation key budget exceeded')
            times.append(float(tb)); values.append(weights(vb)); bounds.append(bound)

    for (ta, va), (tb, vb) in zip(keys[:-1], keys[1:]):
        fractions = [0., 1.]
        if va != vb:
            fractions += [(k-va)/(vb-va) for k in axis[1:-1] if min(va,vb) < k < max(va,vb)]
        fractions.sort()
        for start, end in zip(fractions[:-1], fractions[1:]):
            interval(ta+(tb-ta)*start, va+(vb-va)*start,
                     ta+(tb-ta)*end, va+(vb-va)*end)
    times, values = np.asarray(times), np.asarray(values)
    keep = {0, len(times)-1}
    pending = [(0, len(times)-1)]
    reduction_bound = 0.
    while pending:
        start, end = pending.pop()
        if end-start <= 1:
            continue
        fraction = (times[start+1:end]-times[start]) / (times[end]-times[start])
        predicted = values[start] + fraction[:, None]*(values[end]-values[start])
        difference = values[start+1:end]-predicted
        # A chunk bounds diagnostic memory for dense source curves.
        errors = []
        for offset in range(0, len(difference), 128):
            delta = np.einsum('ij,jkl->ikl', difference[offset:offset+128], shapes)
            errors.extend(np.linalg.norm(delta, axis=2).max(axis=1))
        maximum = float(max(errors))
        if maximum <= error_cm/2:
            reduction_bound = max(reduction_bound, maximum)
        else:
            split = start+1+int(np.argmax(errors))
            keep.add(split)
            pending.extend(((start,split),(split,end)))
    indices = sorted(keep)
    result = values[indices]
    rounded = result.astype(np.float32).astype(float)
    rounding_bound = float(np.linalg.norm(np.einsum('ij,jkl->ikl', rounded-result, shapes), axis=2).max())
    bound = max(bounds)+reduction_bound+rounding_bound
    if bound > error_cm+1e-5 or np.max(np.abs(rounded)) > 4:
        raise ValueError('Baked facial curve exceeds error or runtime weight bounds')
    return times[indices], rounded, dict(source_keys=len(keys), intermediate_keys=len(times),
        output_keys_per_curve=len(indices), spline_bound_cm=max(bounds),
        reduction_bound_cm=reduction_bound, float32_bound_cm=rounding_bound,
        conversion_bound_cm=bound)

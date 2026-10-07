"""Compress sampled facial corrections with a measured per-vertex error bound."""
import numpy as np


def filtered_shapes(samples, threshold, gain_limit=1.):
    samples=np.asarray(samples,dtype=np.float64)
    if (samples.ndim!=3 or samples.shape[2]!=3 or not samples.size or
            not np.isfinite(samples).all() or not np.isfinite(threshold) or threshold<0 or
            not np.isfinite(gain_limit) or not 1<=gain_limit<=32):
        raise ValueError('Invalid morph filtering inputs')
    lengths=np.linalg.norm(samples,axis=2)
    gains=np.maximum(1,np.minimum(gain_limit,16/np.maximum(lengths.max(axis=1),1e-12)))
    return np.where((lengths*gains[:,None]>threshold)[:,:,None],samples,0),gains


def compact_basis(samples, max_error, max_shapes, method='svd'):
    samples = np.asarray(samples, dtype=np.float64)
    if (samples.ndim != 3 or samples.shape[2] != 3 or not samples.size or
            not np.isfinite(samples).all() or not np.isfinite(max_error) or
            max_error <= 0 or type(max_shapes) is not int or max_shapes < 1):
        raise ValueError('Invalid correction samples or limits')
    matrix = samples.reshape((len(samples), -1))
    if method == 'greedy':
        return greedy_basis(samples, max_error, max_shapes)
    if method != 'svd':
        raise ValueError('Unknown facial basis method')
    u, singular, vt = np.linalg.svd(matrix, full_matrices=False)
    residual = matrix.copy()
    for count in range(1, min(max_shapes, len(singular)) + 1):
        residual -= np.outer(u[:, count-1] * singular[count-1], vt[count-1])
        error = float(np.linalg.norm(residual.reshape(samples.shape), axis=2).max())
        if error <= max_error:
            coefficients = u[:, :count] * singular[:count]
            # Keep authored runtime curve weights within [-1, 1].
            scale = np.max(np.abs(coefficients), axis=0)
            scale[scale == 0] = 1
            coefficients /= scale
            basis = (vt[:count] * scale[:, None]).reshape((count, *samples.shape[1:]))
            reconstructed = (coefficients @ basis.reshape((count, -1))).reshape(samples.shape)
            actual = float(np.linalg.norm(samples-reconstructed, axis=2).max())
            if actual > max_error:
                raise ValueError('Independent basis reconstruction exceeds the limit')
            return basis, coefficients, actual
    raise ValueError(f'Correction budget of {max_shapes} shapes exceeds {max_error} cm')


def greedy_basis(samples, max_error, max_shapes):
    """Use worst-point pivots for long clips, checking every sample after each projection."""
    matrix = samples.reshape((len(samples), -1))
    residual = matrix.copy()
    directions, columns = [], []
    for _ in range(max_shapes):
        view = residual.reshape(samples.shape)
        errors = np.einsum('ijk,ijk->ij', view, view)
        sample = int(np.unravel_index(np.argmax(errors), errors.shape)[0])
        direction = residual[sample].copy()
        # Reorthogonalization prevents later pivots from changing earlier coefficients.
        for _ in range(2):
            for prior in directions:
                direction -= np.dot(direction, prior)*prior
        length = np.linalg.norm(direction)
        if length < 1e-12:
            if not directions and float(errors.max()) <= max_error**2:
                return np.zeros((1, *samples.shape[1:])), np.zeros((len(samples), 1)), float(np.sqrt(errors.max()))
            raise ValueError('Facial basis lost rank before meeting the error bound')
        direction /= length
        column = matrix@direction
        for start in range(0, len(samples), 256):
            residual[start:start+256] -= np.outer(column[start:start+256], direction)
        directions.append(direction)
        columns.append(column)
        view = residual.reshape(samples.shape)
        error = float(np.sqrt(np.einsum('ijk,ijk->ij', view, view).max()))
        if error <= max_error:
            weights = np.stack(columns, axis=1)
            scales = np.max(np.abs(weights), axis=0)
            scales[scales == 0] = 1
            basis = (np.stack(directions)*scales[:, None]).reshape((len(directions), *samples.shape[1:]))
            weights /= scales
            actual = 0.
            for start in range(0, len(samples), 256):
                expected = samples[start:start+256]
                restored = (weights[start:start+256]@basis.reshape((len(basis), -1))).reshape(expected.shape)
                actual = max(actual, float(np.linalg.norm(restored-expected, axis=2).max()))
            if actual > max_error:
                raise ValueError('Independent greedy-basis reconstruction exceeds the limit')
            return basis, weights, actual
    raise ValueError(f'Correction budget of {max_shapes} shapes exceeds {max_error} cm')

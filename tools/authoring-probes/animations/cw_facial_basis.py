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


def compact_basis(samples, max_error, max_shapes):
    samples = np.asarray(samples, dtype=np.float64)
    if (samples.ndim != 3 or samples.shape[2] != 3 or not samples.size or
            not np.isfinite(samples).all() or not np.isfinite(max_error) or
            max_error <= 0 or type(max_shapes) is not int or max_shapes < 1):
        raise ValueError('Invalid correction samples or limits')
    matrix = samples.reshape((len(samples), -1))
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

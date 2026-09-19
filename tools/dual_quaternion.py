"""Dual quaternion skinning: blend rigid motions without collapsing the joints.

Linear blend skinning averages *positions*. Halfway between two bones that
disagree by a large rotation, that average falls towards the joint centre, so
the surface sucks inwards. On a shoulder taken from a T-pose to an A-pose, a
55 degree turn, the armpit caves in and the deltoid develops a gouge. No amount
of reweighting fixes it, because the averaging itself is what is wrong: the
straight line between two points on a circle does not stay on the circle.

Dual quaternions average the *motions* instead. A rigid motion is a screw (a
rotation about an axis plus a slide along it) and a unit dual quaternion is a
screw, so blending them and renormalising gives another screw. The surface then
sweeps round the joint rather than cutting across it.

The known cost is the bulge that appears where a joint bends very far, which is
the opposite failure and a much smaller one at these angles.

    q = (real, dual), both quaternions stored [x, y, z, w]
"""
from __future__ import annotations

import numpy as np


def matrix_to_quaternion(rotation: np.ndarray) -> np.ndarray:
    """Rotation matrix to [x, y, z, w], via the largest diagonal term.

    Picking the branch by the largest term avoids the division by a near-zero
    that the naive trace formula hits at 180 degrees.
    """
    m = rotation
    trace = m[0, 0] + m[1, 1] + m[2, 2]
    if trace > 0:
        s = np.sqrt(trace + 1.0) * 2
        return np.array([(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s,
                         (m[1, 0] - m[0, 1]) / s, 0.25 * s])
    if m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = np.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        return np.array([0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s,
                         (m[2, 1] - m[1, 2]) / s])
    if m[1, 1] > m[2, 2]:
        s = np.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        return np.array([(m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s,
                         (m[0, 2] - m[2, 0]) / s])
    s = np.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
    return np.array([(m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s,
                     (m[1, 0] - m[0, 1]) / s])


def multiply(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Hamilton product of two [x, y, z, w] quaternions, broadcasting over rows."""
    ax, ay, az, aw = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    bx, by, bz, bw = b[..., 0], b[..., 1], b[..., 2], b[..., 3]
    return np.stack([aw * bx + ax * bw + ay * bz - az * by,
                     aw * by - ax * bz + ay * bw + az * bx,
                     aw * bz + ax * by - ay * bx + az * bw,
                     aw * bw - ax * bx - ay * by - az * bz], axis=-1)


def from_rigid(rotation: np.ndarray, translation: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Rotation matrix and translation to a unit dual quaternion."""
    real = matrix_to_quaternion(rotation)
    real = real / np.linalg.norm(real)
    pure = np.array([translation[0], translation[1], translation[2], 0.0])
    return real, 0.5 * multiply(pure, real)


def blend(reals: np.ndarray, duals: np.ndarray, weights: np.ndarray,
          pivot: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Weighted sum of dual quaternions, sign-corrected against a pivot.

    A quaternion and its negation are the same rotation, so a naive sum of two
    that happen to be stored with opposite signs cancels instead of averaging,
    and the vertex flies to the origin. Every term is flipped to the same
    hemisphere as the heaviest influence first.
    """
    signs = np.where((reals * pivot[:, None, :]).sum(axis=2) < 0, -1.0, 1.0)
    scaled = weights * signs
    real = (reals * scaled[:, :, None]).sum(axis=1)
    dual = (duals * scaled[:, :, None]).sum(axis=1)
    length = np.maximum(np.linalg.norm(real, axis=1, keepdims=True), 1e-12)
    return real / length, dual / length


def apply(real: np.ndarray, dual: np.ndarray, points: np.ndarray) -> np.ndarray:
    """Transform points by per-point unit dual quaternions."""
    x, y, z, w = real[:, 0], real[:, 1], real[:, 2], real[:, 3]
    dx, dy, dz, dw = dual[:, 0], dual[:, 1], dual[:, 2], dual[:, 3]
    # Rotate: p + 2 * cross(r, cross(r, p) + w * p)
    cross = np.cross(real[:, :3], points) + w[:, None] * points
    rotated = points + 2.0 * np.cross(real[:, :3], cross)
    # Translate: 2 * (w * d_xyz - d_w * r_xyz + cross(r_xyz, d_xyz))
    translation = 2.0 * (w[:, None] * np.stack([dx, dy, dz], axis=1)
                         - dw[:, None] * real[:, :3]
                         + np.cross(real[:, :3], np.stack([dx, dy, dz], axis=1)))
    return rotated + translation

"""Find genuinely inverted geometry, independent of how the mesh was posed.

`shape_transfer.mesh_quality` compares each triangle's normal before and after,
which is the right question when a mesh is reshaped in place. It is the wrong
question after a pose change: a thumb that rotates 110 degrees between a T-pose
and an A-pose has every one of its triangles turn more than 90 degrees, and all
of them are reported as flipped although the thumb is perfectly intact. Chasing
those wastes time on the parts of the fit that are working.

An inversion is a local property, so it is measured locally: a triangle is
inverted when its own normal opposes the surface its neighbours agree on. The
vertex normal field is the average of the surrounding face normals, so comparing
a face against the vertex normals it shares finds a face pointing the wrong way
regardless of where the mesh has been moved to.

Two other real defects are worth separating out:

  pinched   a triangle that lost most of its area, where skinning has folded the
            surface into itself (the groin and armpit do this)
  spiked    a triangle that gained far more area than its neighbours, where one
            vertex has been thrown out of the surface
"""
from __future__ import annotations

import numpy as np


def face_normals(points: np.ndarray, triangles: np.ndarray) -> np.ndarray:
    a = points[triangles[:, 0]]
    raw = np.cross(points[triangles[:, 1]] - a, points[triangles[:, 2]] - a)
    return raw


def vertex_normals(points: np.ndarray, triangles: np.ndarray) -> np.ndarray:
    """Area-weighted, because a fan of slivers should not outvote a large face."""
    raw = face_normals(points, triangles)
    out = np.zeros_like(points)
    for corner in range(3):
        np.add.at(out, triangles[:, corner], raw)
    return out / np.maximum(np.linalg.norm(out, axis=1, keepdims=True), 1e-12)


def inverted(points: np.ndarray, triangles: np.ndarray) -> np.ndarray:
    """Indices of triangles facing against the surface around them."""
    raw = face_normals(points, triangles)
    lengths = np.linalg.norm(raw, axis=1, keepdims=True)
    unit = raw / np.maximum(lengths, 1e-12)
    smooth = vertex_normals(points, triangles)[triangles].mean(axis=1)
    smooth /= np.maximum(np.linalg.norm(smooth, axis=1, keepdims=True), 1e-12)
    return np.where((unit * smooth).sum(axis=1) < 0.0)[0]


def report(before: np.ndarray, after: np.ndarray, triangles: np.ndarray) -> dict:
    areas_before = np.linalg.norm(face_normals(before, triangles), axis=1) / 2
    areas_after = np.linalg.norm(face_normals(after, triangles), axis=1) / 2
    safe = np.maximum(areas_before, 1e-12)
    ratio = areas_after / safe
    folded = inverted(after, triangles)
    was_folded = inverted(before, triangles)
    # Some inversions are in the source already (Genesis 2 tucks the genital
    # surface inside itself), so only the ones this step introduced are ours.
    introduced = np.setdiff1d(folded, was_folded)
    return {'triangles': int(len(triangles)),
            'inverted_before': int(len(was_folded)),
            'inverted_after': int(len(folded)),
            'inversions_introduced': int(len(introduced)),
            'pinched': int((ratio < 0.15).sum()),
            'spiked': int((ratio > 8.0).sum()),
            'area_ratio_median': round(float(np.median(ratio)), 4)}


def relax(points: np.ndarray, triangles: np.ndarray, edges: np.ndarray,
          rounds: int = 40, ring: int = 2, strength: float = 0.6) -> tuple[np.ndarray, int]:
    """Smooth only the vertices caught in inverted triangles, until they let go.

    Only the offending neighbourhood moves, so the rest of the fit is untouched.
    The ring is widened by a couple of steps because a fold is rarely one
    triangle: relaxing the exact triangle and nothing around it just moves the
    crease one row over.
    """
    points = points.copy()
    neighbours: dict[int, list[int]] = {}
    for a, b in edges:
        neighbours.setdefault(int(a), []).append(int(b))
        neighbours.setdefault(int(b), []).append(int(a))

    for _ in range(rounds):
        folded = inverted(points, triangles)
        if len(folded) == 0:
            break
        moving = set(np.unique(triangles[folded]).tolist())
        for _ in range(ring):
            moving |= {n for v in list(moving) for n in neighbours.get(v, [])}
        index = np.array(sorted(moving))
        target = np.array([np.mean(points[neighbours[int(v)]], axis=0)
                           if neighbours.get(int(v)) else points[int(v)] for v in index])
        points[index] = (1 - strength) * points[index] + strength * target
    return points, len(inverted(points, triangles))

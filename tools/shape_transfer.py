"""Carry a shape change from one body onto another with different topology.

The VaM morph library is Genesis 2 Female deltas. The bodies CSS ships are not
G2F, so a morph cannot be applied to them directly. What transfers is not the
deltas but the **deformation field**: the donor's surface before and after the
morph defines how space moves near the body, and the target is re-emitted through
it.

    donor_base ---- morph ----> donor_morphed
         |                            |
      (target vertex attaches here, comes out here)

Each target vertex binds to its closest point on the donor base surface, recorded
as a triangle, barycentric coordinates and an offset in that triangle's local
frame. Re-evaluating the binding against the morphed donor gives the new position.
Vertices further than `max_distance` from the donor keep their place, and the
boundary between the two is blended so no crease appears.

This differs from the collection's existing `transfer_shape.py` in one way that
matters: that tool assumes donor and target are the same body sculpted
differently, so they already sit on top of each other. G2F and a game body do
not, so an alignment step runs first, driven by landmark pairs.

**Landmarks come from the morph library itself.** The vertices a region morph
moves *are* that region, so the centroid of `Busty Breasts 1` is G2F's bust and
the centroid of `MonsterShinkai_Navel` is its navel. No hand-picked indices.

numpy only, no scipy, no Blender. Run it under any python with numpy.
"""
from __future__ import annotations

import numpy as np

# Below this the similarity fit is treated as degenerate rather than silently
# producing a mirrored or collapsed transform.
_MIN_SPREAD = 1e-6


def similarity_transform(source: np.ndarray, target: np.ndarray) -> tuple[np.ndarray, np.ndarray, float]:
    """Umeyama: the rotation, translation and uniform scale taking source onto target.

    Both are (n, 3) landmark sets in matching order. Returns (R, t, s) such that
    `s * points @ R.T + t` maps source space into target space. Uniform scale,
    never a reflection: a body is not improved by being mirrored.
    """
    if source.shape != target.shape or source.shape[0] < 3:
        raise ValueError(f'need at least 3 matching landmarks, got {source.shape} and {target.shape}')
    source_mean, target_mean = source.mean(0), target.mean(0)
    source_centred, target_centred = source - source_mean, target - target_mean
    source_variance = (source_centred ** 2).sum() / len(source)
    if source_variance < _MIN_SPREAD:
        raise ValueError('landmarks are coincident, cannot fit a transform')
    covariance = target_centred.T @ source_centred / len(source)
    u, singular, vt = np.linalg.svd(covariance)
    correction = np.eye(3)
    if np.linalg.det(u) * np.linalg.det(vt) < 0:
        correction[2, 2] = -1.0            # forbid the reflected solution
    rotation = u @ correction @ vt
    scale = float((singular * np.diag(correction)).sum() / source_variance)
    translation = target_mean - scale * rotation @ source_mean
    return rotation, translation, scale


def _triangle_frames(points: np.ndarray, triangles: np.ndarray):
    """Per-triangle origin and orthonormal basis, so an offset survives the morph."""
    a = points[triangles[:, 0]]
    edge1 = points[triangles[:, 1]] - a
    edge2 = points[triangles[:, 2]] - a
    normal = np.cross(edge1, edge2)
    length = np.linalg.norm(normal, axis=1, keepdims=True)
    # A degenerate triangle has no usable frame; give it a stand-in so the maths
    # stays finite and let the distance test discard it.
    safe = np.where(length < 1e-12, 1.0, length)
    normal = normal / safe
    tangent = edge1.copy()
    tangent_length = np.linalg.norm(tangent, axis=1, keepdims=True)
    tangent = tangent / np.where(tangent_length < 1e-12, 1.0, tangent_length)
    bitangent = np.cross(normal, tangent)
    return a, tangent, bitangent, normal


def _closest_on_triangles(query: np.ndarray, a: np.ndarray, b: np.ndarray, c: np.ndarray):
    """Closest point on each triangle to each query point, plus barycentrics.

    Vectorised over a (q, t) pairing. Standard region test from Ericson's
    Real-Time Collision Detection, written branch-free so numpy can carry it.
    """
    ab, ac, ap = b - a, c - a, query - a
    d1 = (ab * ap).sum(-1)
    d2 = (ac * ap).sum(-1)
    bp = query - b
    d3 = (ab * bp).sum(-1)
    d4 = (ac * bp).sum(-1)
    cp = query - c
    d5 = (ab * cp).sum(-1)
    d6 = (ac * cp).sum(-1)
    va = d3 * d6 - d5 * d4
    vb = d5 * d2 - d1 * d6
    vc = d1 * d4 - d3 * d2
    denom = va + vb + vc
    denom = np.where(np.abs(denom) < 1e-20, 1e-20, denom)
    v = np.clip(vb / denom, 0, 1)
    w = np.clip(vc / denom, 0, 1)
    overflow = v + w
    scale = np.where(overflow > 1, overflow, 1.0)
    v, w = v / scale, w / scale
    u = 1.0 - v - w
    point = a + ab * v[..., None] + ac * w[..., None]
    return point, np.stack([u, v, w], axis=-1)


class SurfaceBinding:
    """Where each target vertex sits relative to the donor surface."""

    def __init__(self, triangle: np.ndarray, barycentric: np.ndarray,
                 offset: np.ndarray, distance: np.ndarray):
        self.triangle = triangle
        self.barycentric = barycentric
        self.offset = offset            # in the triangle's own frame
        self.distance = distance

    def evaluate(self, points: np.ndarray, triangles: np.ndarray) -> np.ndarray:
        """Rebuild world positions from this binding against a given donor pose."""
        origin, tangent, bitangent, normal = _triangle_frames(points, triangles)
        tri = self.triangle
        a = points[triangles[tri, 0]]
        b = points[triangles[tri, 1]]
        c = points[triangles[tri, 2]]
        surface = (a * self.barycentric[:, 0:1]
                   + b * self.barycentric[:, 1:2]
                   + c * self.barycentric[:, 2:3])
        return (surface
                + tangent[tri] * self.offset[:, 0:1]
                + bitangent[tri] * self.offset[:, 1:2]
                + normal[tri] * self.offset[:, 2:3])


def bind(target_points: np.ndarray, donor_points: np.ndarray, donor_triangles: np.ndarray,
         cell_size: float | None = None, max_distance: float = 6.0) -> SurfaceBinding:
    """Attach every target vertex to the donor surface.

    `cell_size` is the spatial hash bucket; it defaults to the mean triangle
    size, which keeps candidate counts small without missing the true closest
    triangle. Distances are in the meshes' own units (centimetres here).
    """
    origin, tangent, bitangent, normal = _triangle_frames(donor_points, donor_triangles)
    a = donor_points[donor_triangles[:, 0]]
    b = donor_points[donor_triangles[:, 1]]
    c = donor_points[donor_triangles[:, 2]]
    centroid = (a + b + c) / 3.0

    if cell_size is None:
        extent = np.linalg.norm(np.maximum.reduce([
            np.abs(b - a), np.abs(c - a), np.abs(c - b)]), axis=1)
        cell_size = max(float(np.mean(extent)) * 2.0, 1e-4)

    # Spatial hash of triangle centroids. Searching a widening ring of cells is
    # enough because a triangle's own cell is within one cell of its centroid at
    # this cell size, and the ring grows until something is found.
    keys = np.floor(centroid / cell_size).astype(np.int64)
    table: dict[tuple[int, int, int], list[int]] = {}
    for index, key in enumerate(map(tuple, keys)):
        table.setdefault(key, []).append(index)

    count = len(target_points)
    out_triangle = np.zeros(count, dtype=np.int64)
    out_bary = np.zeros((count, 3))
    out_offset = np.zeros((count, 3))
    out_distance = np.full(count, np.inf)

    max_rings = max(1, int(np.ceil(max_distance / cell_size)) + 1)
    query_keys = np.floor(target_points / cell_size).astype(np.int64)

    for i in range(count):
        point = target_points[i]
        base_key = query_keys[i]
        candidates: list[int] = []
        for ring in range(max_rings):
            if candidates and ring > 1:
                break
            for dx in range(-ring, ring + 1):
                for dy in range(-ring, ring + 1):
                    for dz in range(-ring, ring + 1):
                        if ring and max(abs(dx), abs(dy), abs(dz)) != ring:
                            continue        # only the new shell
                        found = table.get((base_key[0] + dx, base_key[1] + dy, base_key[2] + dz))
                        if found:
                            candidates.extend(found)
        if not candidates:
            continue
        idx = np.asarray(candidates)
        closest, bary = _closest_on_triangles(point[None, :], a[idx], b[idx], c[idx])
        gaps = np.linalg.norm(closest - point, axis=1)
        best = int(np.argmin(gaps))
        triangle = int(idx[best])
        gap = float(gaps[best])
        if gap > max_distance:
            out_distance[i] = gap
            out_triangle[i] = triangle
            out_bary[i] = bary[best]
            continue
        delta = point - closest[best]
        out_triangle[i] = triangle
        out_bary[i] = bary[best]
        out_offset[i] = [float(delta @ tangent[triangle]),
                         float(delta @ bitangent[triangle]),
                         float(delta @ normal[triangle])]
        out_distance[i] = gap

    return SurfaceBinding(out_triangle, out_bary, out_offset, out_distance)


def transfer(target_points: np.ndarray, donor_base: np.ndarray, donor_morphed: np.ndarray,
             donor_triangles: np.ndarray, max_distance: float = 6.0,
             falloff: float = 2.0, binding: SurfaceBinding | None = None):
    """Move `target_points` by the donor's shape change.

    Returns (moved_points, report). Vertices beyond `max_distance` stay put, and
    the last `falloff` units before that cutoff fade smoothly to zero so the
    treated and untreated regions meet without a crease.
    """
    if binding is None:
        binding = bind(target_points, donor_base, donor_triangles, max_distance=max_distance)

    before = binding.evaluate(donor_base, donor_triangles)
    after = binding.evaluate(donor_morphed, donor_triangles)
    displacement = after - before

    # smoothstep from full strength at (max_distance - falloff) to zero at max_distance
    distance = binding.distance
    inner = max(max_distance - falloff, 0.0)
    span = max(max_distance - inner, 1e-9)
    t = np.clip((distance - inner) / span, 0.0, 1.0)
    weight = 1.0 - (t * t * (3.0 - 2.0 * t))
    weight[~np.isfinite(distance)] = 0.0
    displacement = displacement * weight[:, None]

    moved = target_points + displacement
    lengths = np.linalg.norm(displacement, axis=1)
    return moved, {
        'moved_vertices': int((lengths > 1e-6).sum()),
        'total_vertices': int(len(target_points)),
        'largest_move': float(lengths.max()) if len(lengths) else 0.0,
        'mean_move': float(lengths[lengths > 1e-6].mean()) if (lengths > 1e-6).any() else 0.0,
        'unbound_vertices': int((~np.isfinite(distance)).sum()),
        'beyond_max_distance': int((distance > max_distance).sum()),
    }


def region_centroid(points: np.ndarray, indices) -> np.ndarray:
    """Centroid of the vertices a region morph moves, used as an automatic landmark."""
    idx = np.asarray(list(indices), dtype=np.int64)
    if not len(idx):
        raise ValueError('empty region')
    if idx.max() >= len(points):
        raise ValueError(f'region indexes vertex {idx.max()} but the mesh has {len(points)}')
    return points[idx].mean(0)


def _closest_points_on_surface(query: np.ndarray, donor_points: np.ndarray,
                               donor_triangles: np.ndarray, subset: np.ndarray | None = None):
    """Brute-force closest surface point for a small query set.

    Used by `conform_region`, where the query is one anatomical region (hundreds
    of vertices) rather than a whole body, so the simple version is both fast
    enough and free of the spatial-hash edge cases.
    """
    a = donor_points[donor_triangles[:, 0]]
    b = donor_points[donor_triangles[:, 1]]
    c = donor_points[donor_triangles[:, 2]]
    if subset is not None:
        a, b, c = a[subset], b[subset], c[subset]
    best_point = np.zeros_like(query)
    best_gap = np.full(len(query), np.inf)
    chunk = max(1, 2_000_000 // max(len(a), 1))
    for start in range(0, len(query), chunk):
        block = query[start:start + chunk]
        point, _ = _closest_on_triangles(block[:, None, :], a[None], b[None], c[None])
        gaps = np.linalg.norm(point - block[:, None, :], axis=-1)
        pick = np.argmin(gaps, axis=1)
        rows = np.arange(len(block))
        best_point[start:start + chunk] = point[rows, pick]
        best_gap[start:start + chunk] = gaps[rows, pick]
    return best_point, best_gap


def conform_region(target_points: np.ndarray, donor_points: np.ndarray,
                   donor_triangles: np.ndarray, centre: np.ndarray, radius: float,
                   strength: float = 1.0, feather: float = 0.35,
                   donor_subset: np.ndarray | None = None):
    """Pull one region of the target onto the donor's surface, feathered at the edge.

    This is what the figure transfer must not do and what anatomy needs. A morph
    transfer moves a region *by* a change; this moves a region *onto* a shape,
    which is the only way to give a body an areola it never had.

    It is also why the current Beaute pipeline loses the nipple. That one runs a
    robust median filter over a 5 cm neighbourhood to reject the Genessa
    undersuit's straps as outliers, and a nipple is a small high-curvature
    feature that the same filter treats as an outlier and flattens. Here nothing
    is smoothed inside the region at all: the feather is a blend weight on the
    displacement, applied from `radius * (1 - feather)` outward, so the region
    meets the surrounding skin without a seam while its interior keeps every bit
    of the donor's detail.

    `donor_subset` restricts which donor triangles may be matched, so a nipple
    cannot accidentally snap onto the arm resting beside it.
    """
    if radius <= 0:
        raise ValueError('radius must be positive')
    if not 0.0 <= feather < 1.0:
        raise ValueError('feather must be in [0, 1)')

    centre = np.asarray(centre, dtype=np.float64)
    distance_to_centre = np.linalg.norm(target_points - centre, axis=1)
    inside = np.where(distance_to_centre <= radius)[0]
    if not len(inside):
        return target_points.copy(), {'region_vertices': 0, 'moved_vertices': 0,
                                      'largest_move': 0.0, 'mean_move': 0.0}

    closest, gap = _closest_points_on_surface(
        target_points[inside], donor_points, donor_triangles, donor_subset)

    inner = radius * (1.0 - feather)
    span = max(radius - inner, 1e-9)
    t = np.clip((distance_to_centre[inside] - inner) / span, 0.0, 1.0)
    weight = (1.0 - (t * t * (3.0 - 2.0 * t))) * strength

    moved = target_points.copy()
    displacement = (closest - target_points[inside]) * weight[:, None]
    moved[inside] = target_points[inside] + displacement

    lengths = np.linalg.norm(displacement, axis=1)
    return moved, {
        'region_vertices': int(len(inside)),
        'moved_vertices': int((lengths > 1e-6).sum()),
        'largest_move': float(lengths.max()),
        'mean_move': float(lengths.mean()),
        'largest_gap_before': float(gap.max()),
    }


def select_region(points: np.ndarray, centre, radius: float) -> np.ndarray:
    """Indices of vertices within `radius` of `centre`."""
    return np.where(np.linalg.norm(points - np.asarray(centre, dtype=np.float64), axis=1) <= radius)[0]


def vertex_normals(points: np.ndarray, triangles: np.ndarray) -> np.ndarray:
    """Area-weighted vertex normals."""
    a = points[triangles[:, 0]]
    face = np.cross(points[triangles[:, 1]] - a, points[triangles[:, 2]] - a)
    normals = np.zeros_like(points)
    for column in range(3):
        np.add.at(normals, triangles[:, column], face)
    length = np.linalg.norm(normals, axis=1, keepdims=True)
    return normals / np.where(length < 1e-12, 1.0, length)


def _ray_hits(origins: np.ndarray, directions: np.ndarray, a: np.ndarray,
              b: np.ndarray, c: np.ndarray, limit: float, facing: float = 0.3):
    """Nearest ray/triangle hit within +/- limit, Moller-Trumbore, both directions.

    Returns (distance_along_ray, hit_mask). A ray is allowed to hit behind its
    origin because a target vertex may start either inside or outside the donor
    surface, and both need pulling onto it.

    `facing` rejects hits on a donor triangle whose normal disagrees with the ray
    direction by more than that cosine. Without it a breast vertex can snap to
    the *back* of the donor's chest, or to the far side of a fold, and
    neighbouring vertices then land on different sheets of the surface. That
    tears the mesh into spikes while every per-vertex number still looks
    reasonable, which is exactly what happened on the first run.
    """
    edge1, edge2 = b - a, c - a
    pvec = np.cross(directions[:, None, :], edge2[None])
    det = (edge1[None] * pvec).sum(-1)
    parallel = np.abs(det) < 1e-12
    safe = np.where(parallel, 1.0, det)
    tvec = origins[:, None, :] - a[None]
    u = (tvec * pvec).sum(-1) / safe
    qvec = np.cross(tvec, edge1[None])
    v = (directions[:, None, :] * qvec).sum(-1) / safe
    t = (edge2[None] * qvec).sum(-1) / safe
    # Donor triangle normals, compared against each ray's own direction.
    face_normal = np.cross(edge1, edge2)
    face_len = np.linalg.norm(face_normal, axis=1, keepdims=True)
    face_normal = face_normal / np.where(face_len < 1e-12, 1.0, face_len)
    alignment = np.abs((directions[:, None, :] * face_normal[None]).sum(-1))

    ok = ((~parallel) & (u >= -1e-9) & (v >= -1e-9) & (u + v <= 1 + 1e-9)
          & (np.abs(t) <= limit) & (alignment >= facing))
    scored = np.where(ok, np.abs(t), np.inf)
    pick = np.argmin(scored, axis=1)
    rows = np.arange(len(origins))
    best = scored[rows, pick]
    return t[rows, pick], np.isfinite(best)


def mesh_quality(before: np.ndarray, after: np.ndarray, triangles: np.ndarray) -> dict:
    """Did an edit tear the mesh? Compares triangle normals and areas.

    A per-vertex measurement cannot see a tear: vertices can each land at a
    plausible height while neighbours land on opposite sides of a fold, which
    reads fine numerically and renders as spikes. This compares the surface
    before and after, so a flipped or collapsed triangle is caught.
    """
    def normals_and_areas(points):
        a = points[triangles[:, 0]]
        cross = np.cross(points[triangles[:, 1]] - a, points[triangles[:, 2]] - a)
        area = np.linalg.norm(cross, axis=1)
        safe = np.where(area < 1e-20, 1.0, area)
        return cross / safe[:, None], area / 2.0

    n0, a0 = normals_and_areas(before)
    n1, a1 = normals_and_areas(after)
    agreement = (n0 * n1).sum(1)
    touched = a0 > 0
    growth = np.where(a0 > 1e-20, a1 / np.where(a0 > 1e-20, a0, 1.0), 1.0)
    return {
        'flipped_triangles': int((agreement < 0).sum()),
        'sharply_turned': int((agreement < 0.5).sum()),
        'collapsed_triangles': int(((a1 < a0 * 0.05) & touched).sum()),
        'exploded_triangles': int((growth > 5.0).sum()),
        'worst_turn_degrees': float(np.degrees(np.arccos(np.clip(agreement.min(), -1, 1)))),
        'triangles': int(len(triangles)),
    }


def conform_region_along_normal(target_points: np.ndarray, target_triangles: np.ndarray,
                                donor_points: np.ndarray, donor_triangles: np.ndarray,
                                centre, radius: float, strength: float = 1.0,
                                feather: float = 0.35, search: float | None = None,
                                facing: float = 0.3):
    """Conform a region by sliding each vertex along its own normal onto the donor.

    Preferred over the nearest-point form for anatomy. Nearest-point is ambiguous
    exactly where detail lives: for a vertex sitting inside a bulge, the closest
    donor surface point is on the *side* of the bulge rather than its tip, so a
    nipple conformed that way comes out squashed sideways. Projecting along the
    vertex normal keeps the correspondence a human would expect, and leaves the
    vertex's tangential position alone so the target's UVs do not shear.

    Vertices whose normal misses the donor within `search` keep their place.
    """
    if radius <= 0:
        raise ValueError('radius must be positive')
    centre = np.asarray(centre, dtype=np.float64)
    search = search if search is not None else radius

    distance_to_centre = np.linalg.norm(target_points - centre, axis=1)
    inside = np.where(distance_to_centre <= radius)[0]
    if not len(inside):
        return target_points.copy(), {'region_vertices': 0, 'moved_vertices': 0,
                                      'largest_move': 0.0, 'mean_move': 0.0, 'missed': 0}

    normals = vertex_normals(target_points, target_triangles)
    a = donor_points[donor_triangles[:, 0]]
    b = donor_points[donor_triangles[:, 1]]
    c = donor_points[donor_triangles[:, 2]]
    # Only donor triangles near the region can matter; this keeps the ray test small.
    centroid = (a + b + c) / 3.0
    nearby = np.where(np.linalg.norm(centroid - centre, axis=1) <= radius * 2.5)[0]
    if not len(nearby):
        return target_points.copy(), {'region_vertices': int(len(inside)), 'moved_vertices': 0,
                                      'largest_move': 0.0, 'mean_move': 0.0,
                                      'missed': int(len(inside))}

    travel, hit = _ray_hits(target_points[inside], normals[inside],
                            a[nearby], b[nearby], c[nearby], search, facing=facing)

    inner = radius * (1.0 - feather)
    span = max(radius - inner, 1e-9)
    t = np.clip((distance_to_centre[inside] - inner) / span, 0.0, 1.0)
    weight = (1.0 - (t * t * (3.0 - 2.0 * t))) * strength
    weight = np.where(hit, weight, 0.0)

    displacement = normals[inside] * (travel * weight)[:, None]
    moved = target_points.copy()
    moved[inside] = target_points[inside] + displacement

    lengths = np.linalg.norm(displacement, axis=1)
    return moved, {
        'region_vertices': int(len(inside)),
        'moved_vertices': int((lengths > 1e-6).sum()),
        'largest_move': float(lengths.max()) if len(lengths) else 0.0,
        'mean_move': float(lengths[lengths > 1e-6].mean()) if (lengths > 1e-6).any() else 0.0,
        'missed': int((~hit).sum()),
    }


def _adjacency(triangles: np.ndarray, count: int):
    """Neighbour lists as a flat CSR-style pair, built once and reused."""
    edges = np.vstack([triangles[:, [0, 1]], triangles[:, [1, 2]], triangles[:, [2, 0]]])
    edges = np.vstack([edges, edges[:, ::-1]])
    order = np.lexsort((edges[:, 1], edges[:, 0]))
    edges = edges[order]
    keep = np.ones(len(edges), dtype=bool)
    keep[1:] = (edges[1:] != edges[:-1]).any(1)
    edges = edges[keep]
    starts = np.searchsorted(edges[:, 0], np.arange(count + 1))
    return edges[:, 1], starts


def smooth_region(points: np.ndarray, triangles: np.ndarray, indices: np.ndarray,
                  iterations: int = 12, rate: float = 0.5) -> np.ndarray:
    """Laplacian-smooth just the given vertices, leaving the rest pinned.

    Used to recover the surface a region *would* have without its detail, so the
    detail can then be scaled relative to it.
    """
    neighbours, starts = _adjacency(triangles, len(points))
    out = points.copy()
    mask = np.zeros(len(points), dtype=bool)
    mask[indices] = True
    for _ in range(iterations):
        averaged = out.copy()
        for i in indices:
            lo, hi = starts[i], starts[i + 1]
            if hi > lo:
                averaged[i] = out[neighbours[lo:hi]].mean(0)
        out[mask] = out[mask] + rate * (averaged[mask] - out[mask])
    return out


def scale_relief(points: np.ndarray, triangles: np.ndarray, centre, radius: float,
                 factor: float, feather: float = 0.35, iterations: int = 12):
    """Scale a region's detail toward or away from its own smoothed surface.

    The honest way to tame a feature that is simply too proud. A smoothed copy of
    the region is the surface without its detail; every vertex is then placed at
    `smooth + (original - smooth) * factor`, so `factor` 0.6 keeps 60% of the
    relief and 1.4 exaggerates it.

    This cannot tear the mesh, because every vertex moves along its own offset
    from a surface its neighbours share, rather than being projected onto a
    second surface that may fold differently. Projecting a breast onto a donor
    breast of another shape is what produced 38 flipped triangles and a spiked
    render; this does not have that failure mode.
    """
    if radius <= 0:
        raise ValueError('radius must be positive')
    centre = np.asarray(centre, dtype=np.float64)
    distance = np.linalg.norm(points - centre, axis=1)
    inside = np.where(distance <= radius)[0]
    if not len(inside):
        return points.copy(), {'region_vertices': 0, 'moved_vertices': 0, 'largest_move': 0.0}

    smoothed = smooth_region(points, triangles, inside, iterations=iterations)
    inner = radius * (1.0 - feather)
    span = max(radius - inner, 1e-9)
    t = np.clip((distance[inside] - inner) / span, 0.0, 1.0)
    blend = 1.0 - (t * t * (3.0 - 2.0 * t))
    effective = 1.0 + (factor - 1.0) * blend

    detail = points[inside] - smoothed[inside]
    out = points.copy()
    out[inside] = smoothed[inside] + detail * effective[:, None]
    moved = np.linalg.norm(out[inside] - points[inside], axis=1)
    return out, {
        'region_vertices': int(len(inside)),
        'moved_vertices': int((moved > 1e-6).sum()),
        'largest_move': float(moved.max()) if len(moved) else 0.0,
        'mean_move': float(moved.mean()) if len(moved) else 0.0,
    }

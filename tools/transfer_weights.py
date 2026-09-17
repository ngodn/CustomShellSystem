"""Give a fitted mesh the game's own skin weights by copying them off the game mesh.

Once Genesis 2 has been fitted onto Mortal Shell II's skeleton it occupies the
same space as the shipped Genessa body, so the weights can simply be read off
that body rather than invented. This matters more than it sounds: hand-rolled
weights look fine in a rest pose and fall apart the first time an elbow bends,
whereas the shipped weights are the ones every animation in the game was
authored against.

Each target vertex takes an inverse-distance blend of the nearest source
vertices. Blending several rather than copying the single nearest one matters at
material seams, where the source mesh has two coincident vertices that can carry
different weights, so "nearest" is a coin toss and the result speckles.

Two adjustments are applied afterwards.

**Jiggle boost.** The shipped body caps the spring bones at 0.184 for the
breasts, 0.165 for the buttocks and 0.122 for the belly, so the springs only ever
move the surface by about a sixth of their travel. Boosting those channels and
renormalising gives the same springs visibly more to do without retuning them.

**Graph smoothing.** A light pass over the mesh edges, because the transfer is
per-vertex and can leave single-vertex speckles that show up as one pinched
triangle under animation.

    tools/transfer_weights.py --source SK_Sester_Genessa_V6.glb \
        --target work/v1.0.0-body/fit/g2f_fitted.obj --out work/v1.0.0-body/skin
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

import gltf_mesh

# How much harder the secondary motion bones pull on the authored body than on
# the shipped one. Applied before renormalising, so a boost of 2 does not mean
# twice the weight, it means twice the share of the competition.
DEFAULT_BOOST = {'brust001': 2.2, 'brust002': 2.2, 'butt001': 2.0, 'butt002': 2.0,
                 'belly': 1.6}
MAX_INFLUENCES = 8


def read_obj_points(path):
    points, faces = [], []
    for line in Path(path).read_text().splitlines():
        if line.startswith('v '):
            points.append([float(v) for v in line.split()[1:4]])
        elif line.startswith('f '):
            faces.append([int(t.split('/')[0]) - 1 for t in line.split()[1:]])
    return np.asarray(points, dtype=np.float64), faces


def nearest_blend(target: np.ndarray, source: np.ndarray, joints: np.ndarray,
                  weights: np.ndarray, bone_count: int, neighbours: int = 6,
                  chunk: int = 512):
    """Accumulate a dense per-bone weight row for every target vertex."""
    out = np.zeros((len(target), bone_count))
    closest = np.zeros(len(target))
    for start in range(0, len(target), chunk):
        block = target[start:start + chunk]
        gaps = np.linalg.norm(block[:, None, :] - source[None, :, :], axis=2)
        take = np.argpartition(gaps, neighbours, axis=1)[:, :neighbours]
        rows = np.arange(len(block))[:, None]
        distance = gaps[rows, take]
        closest[start:start + chunk] = distance.min(axis=1)
        # Inverse square distance, floored so a coincident vertex does not become
        # infinitely important and wipe out its neighbours.
        share = 1.0 / np.maximum(distance, 1e-4) ** 2
        share /= share.sum(axis=1, keepdims=True)
        for k in range(neighbours):
            picked = take[:, k]
            contribution = share[:, k][:, None] * weights[picked]
            np.add.at(out[start:start + chunk], (rows.repeat(joints.shape[1], axis=1),
                                                 joints[picked]), contribution)
    return out, closest


def smooth(weights, edges, rounds=3, strength=0.35):
    counts = np.zeros(len(weights))
    for a, b in edges:
        counts[a] += 1
        counts[b] += 1
    busy = counts > 0
    for _ in range(rounds):
        totals = np.zeros_like(weights)
        for a, b in edges:
            totals[a] += weights[b]
            totals[b] += weights[a]
        totals[busy] /= counts[busy, None]
        weights[busy] = (1 - strength) * weights[busy] + strength * totals[busy]
        weights /= np.maximum(weights.sum(axis=1, keepdims=True), 1e-12)
    return weights


def trim(weights, limit=MAX_INFLUENCES):
    """Keep the strongest influences per vertex and renormalise to exactly one."""
    if weights.shape[1] > limit:
        cutoff = np.partition(weights, -limit, axis=1)[:, -limit][:, None]
        weights = np.where(weights >= cutoff, weights, 0.0)
        # A tie on the cutoff can leave more than `limit` survivors; drop the
        # extras by rank so the influence count is never over the importer's cap.
        excess = np.where((weights > 0).sum(axis=1) > limit)[0]
        for row in excess:
            order = np.argsort(-weights[row])
            weights[row, order[limit:]] = 0.0
    return weights / np.maximum(weights.sum(axis=1, keepdims=True), 1e-12)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--source', required=True, type=Path, help='skinned .glb')
    parser.add_argument('--target', required=True, type=Path, help='fitted .obj')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--neighbours', type=int, default=6)
    parser.add_argument('--no-boost', action='store_true')
    args = parser.parse_args(argv)

    source = gltf_mesh.load(args.source)
    if not source.is_skinned:
        raise SystemExit(f'{args.source} has no skin weights')
    target, faces = read_obj_points(args.target)

    dense, closest = nearest_blend(target, source.points, source.joints, source.weights,
                                   len(source.bone_names), args.neighbours)

    boosted = {}
    if not args.no_boost:
        for name, factor in DEFAULT_BOOST.items():
            if name in source.bone_names:
                dense[:, source.bone_names.index(name)] *= factor
                boosted[name] = factor
    dense /= np.maximum(dense.sum(axis=1, keepdims=True), 1e-12)

    edges = set()
    for face in faces:
        for i in range(len(face)):
            a, b = face[i], face[(i + 1) % len(face)]
            edges.add((a, b) if a < b else (b, a))
    edges = np.asarray(sorted(edges), dtype=np.int64)
    dense = smooth(dense, edges)
    dense = trim(dense)

    args.out.mkdir(parents=True, exist_ok=True)
    np.save(args.out / 'weights.npy', dense.astype(np.float32))
    (args.out / 'bones.json').write_text(json.dumps(source.bone_names, indent=1) + '\n')

    used = np.where(dense.sum(axis=0) > 0)[0]
    per_vertex = (dense > 0).sum(axis=1)
    report = {'target_vertices': len(target), 'source_vertices': len(source.points),
              'bones_declared': len(source.bone_names), 'bones_used': int(len(used)),
              'influences_per_vertex': {'min': int(per_vertex.min()),
                                        'max': int(per_vertex.max()),
                                        'mean': round(float(per_vertex.mean()), 2)},
              'weight_sum_error': round(float(np.abs(dense.sum(axis=1) - 1).max()), 8),
              'nearest_source_mm': {'median': round(float(np.median(closest)) * 1000, 3),
                                    'p95': round(float(np.percentile(closest, 95)) * 1000, 3),
                                    'max': round(float(closest.max()) * 1000, 3)},
              'boosted': boosted,
              'spring_bone_peak': {n: round(float(dense[:, source.bone_names.index(n)].max()), 4)
                                   for n in DEFAULT_BOOST if n in source.bone_names}}
    (args.out / 'transfer.report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

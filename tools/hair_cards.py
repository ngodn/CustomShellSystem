"""Turn VaM strand hair into hair cards a game engine can render.

VaM draws every strand individually on the GPU. Unreal will not: 834 strands of
25 segments is 20,000 line primitives per head, and MSII has no strand hair
plugin cooked in. The standard answer is hair cards, flat ribbons carrying an
alpha strip that each stand in for a clump of strands.

The conversion is three steps.

1. **Cluster.** Strands are grouped by where they start and where they end, so a
   clump that leaves the same part of the scalp and falls to the same place
   becomes one card. Clustering on the root alone merges strands that part
   company halfway down and gives cards that visibly cut through each other.

2. **Spine.** Each cluster averages to one polyline. Averaging is done after
   resampling every member to the same arc-length parameter, because the raw
   control points are not evenly spaced and a naive average pulls the spine
   toward wherever the source happened to place more points.

3. **Ribbon.** Each spine point gets two vertices offset along a binormal. The
   binormal is the strand tangent crossed with the outward direction from the
   head centre, which keeps the card facing away from the skull instead of
   twisting as the tangent swings past vertical.

Width tapers to a point at the tip, which is what stops cards reading as flat
straps. UVs put the card's own strip of the atlas across u and the length down v.

    tools/hair_cards.py 'Lexi Long (REN).vab' --cards 320 --out work/hair
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from vam_hair import read_hair


def resample(points: np.ndarray, count: int) -> np.ndarray:
    """Resample a polyline to `count` points evenly spaced by arc length."""
    if len(points) < 2:
        return np.repeat(points[:1], count, axis=0)
    steps = np.linalg.norm(np.diff(points, axis=0), axis=1)
    travelled = np.concatenate([[0.0], np.cumsum(steps)])
    if travelled[-1] <= 0:
        return np.repeat(points[:1], count, axis=0)
    wanted = np.linspace(0.0, travelled[-1], count)
    return np.stack([np.interp(wanted, travelled, points[:, axis]) for axis in range(3)], axis=1)


def cluster(features: np.ndarray, count: int, seed: int = 0, rounds: int = 25) -> np.ndarray:
    """k-means with k-means++ seeding. Returns a label per row.

    Plain random seeding leaves whole regions of the scalp without a centre, and
    the resulting cards bunch on one side of the head.
    """
    rng = np.random.default_rng(seed)
    count = min(count, len(features))
    centres = [features[rng.integers(len(features))]]
    distance = np.linalg.norm(features - centres[0], axis=1) ** 2
    for _ in range(count - 1):
        total = distance.sum()
        pick = rng.integers(len(features)) if total <= 0 else \
            int(np.searchsorted(np.cumsum(distance / total), rng.random()))
        centres.append(features[min(pick, len(features) - 1)])
        distance = np.minimum(distance, np.linalg.norm(features - centres[-1], axis=1) ** 2)
    centres = np.asarray(centres)

    labels = np.zeros(len(features), dtype=np.int64)
    for _ in range(rounds):
        gaps = ((features[:, None, :] - centres[None, :, :]) ** 2).sum(axis=2)
        fresh = gaps.argmin(axis=1)
        if np.array_equal(fresh, labels):
            break
        labels = fresh
        for k in range(len(centres)):
            members = features[labels == k]
            if len(members):
                centres[k] = members.mean(axis=0)
    return labels


def build_cards(strands, cards=320, samples=9, width_scale=1.6, seed=0):
    """Return (points, uvs, triangles, report). Units follow the input, metres."""
    # A single strand with a NaN in it is enough to lose the whole head: the
    # k-means++ seeding divides by a NaN total, every centre collapses onto the
    # same point, and the result is one card holding 740 strands. Two of the ten
    # hairs VaM ships have exactly one such strand, so this is not theoretical.
    usable = [s for s in strands if len(s) > 1 and np.isfinite(s).all()]
    dropped = sum(1 for s in strands if len(s) > 1 and not np.isfinite(s).all())
    if not usable:
        raise ValueError('no strands with more than one point')
    resampled = np.stack([resample(s, samples) for s in usable])

    roots, tips = resampled[:, 0, :], resampled[:, -1, :]
    # Root and tip are weighted equally: clustering on the root alone merges
    # strands that share a parting but fall to opposite shoulders.
    labels = cluster(np.concatenate([roots, tips], axis=1), cards, seed=seed)

    head = roots.mean(axis=0)
    points, uvs, triangles, widths = [], [], [], []
    for k in sorted(set(labels.tolist())):
        members = resampled[labels == k]
        if len(members) == 0:
            continue
        spine = members.mean(axis=0)
        # Card half-width is how far the clump actually spreads, not a constant.
        spread = float(np.linalg.norm(members - spine, axis=2).mean())
        half = max(spread * width_scale, 0.004)
        widths.append(half * 2)

        tangents = np.gradient(spine, axis=0)
        tangents /= np.maximum(np.linalg.norm(tangents, axis=1, keepdims=True), 1e-9)
        outward = spine - head
        outward /= np.maximum(np.linalg.norm(outward, axis=1, keepdims=True), 1e-9)
        binormal = np.cross(tangents, outward)
        length = np.linalg.norm(binormal, axis=1, keepdims=True)
        # Where the strand runs straight out from the head the cross product
        # collapses; fall back to a world-up reference so the card keeps a width.
        collapsed = (length < 1e-6).ravel()
        if collapsed.any():
            fallback = np.cross(tangents[collapsed], np.array([0.0, 1.0, 0.0]))
            binormal[collapsed] = fallback
            length[collapsed] = np.maximum(np.linalg.norm(fallback, axis=1, keepdims=True), 1e-9)
        binormal /= length

        # Taper: full width for the first two thirds, then to a point.
        along = np.linspace(0.0, 1.0, samples)
        taper = np.clip((1.0 - along) / 0.34, 0.0, 1.0) ** 0.5
        offset = binormal * (half * taper)[:, None]

        base = len(points)
        for i in range(samples):
            points.append(spine[i] - offset[i])
            points.append(spine[i] + offset[i])
            uvs.append([0.0, along[i]])
            uvs.append([1.0, along[i]])
        for i in range(samples - 1):
            a, b = base + i * 2, base + i * 2 + 1
            c, d = a + 2, b + 2
            triangles.append([a, c, d])
            triangles.append([a, d, b])

    report = {'strands': len(usable), 'dropped_strands': dropped,
              'cards': len(widths), 'samples': samples,
              'vertices': len(points), 'triangles': len(triangles),
              'mean_card_width_mm': round(float(np.mean(widths)) * 1000, 2),
              'strands_per_card': round(len(usable) / max(1, len(widths)), 2)}
    return np.asarray(points), np.asarray(uvs), np.asarray(triangles, dtype=np.int64), report


def write_obj(path, points, uvs, triangles):
    lines = [f'# {len(points)} vertices, {len(triangles)} triangles']
    lines += [f'v {x:.6f} {y:.6f} {z:.6f}' for x, y, z in points]
    lines += [f'vt {u:.6f} {v:.6f}' for u, v in uvs]
    lines += [f'f {a+1}/{a+1} {b+1}/{b+1} {c+1}/{c+1}' for a, b, c in triangles]
    Path(path).write_text('\n'.join(lines) + '\n')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('paths', nargs='+')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--cards', type=int, default=320)
    parser.add_argument('--samples', type=int, default=9, help='points down each card')
    parser.add_argument('--width-scale', type=float, default=1.6)
    args = parser.parse_args(argv)

    targets: list[Path] = []
    for entry in args.paths:
        path = Path(entry)
        targets += sorted(path.rglob('*.vab')) if path.is_dir() else [path]
    args.out.mkdir(parents=True, exist_ok=True)

    report = {}
    for path in targets:
        try:
            hair = read_hair(path)
            points, uvs, triangles, info = build_cards(
                hair.polylines(), args.cards, args.samples, args.width_scale)
        except Exception as exc:
            print(f'  {path.stem[:34]:<36} skipped: {exc}')
            continue
        safe = ''.join(c if c.isalnum() or c in ' _.-' else '_' for c in path.stem)
        write_obj(args.out / f'{safe}.cards.obj', points, uvs, triangles)
        info['scalp'] = hair.scalp
        report[path.stem] = info
        print(f'  {path.stem[:30]:<32} {info["cards"]:>4} cards  {info["triangles"]:>6} tris  '
              f'{info["strands_per_card"]:>5.1f} strands/card  width {info["mean_card_width_mm"]:>5.1f} mm')
    (args.out / 'cards.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

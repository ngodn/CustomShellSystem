"""Stabilize contact IK poles only where the original leg is nearly straight."""
import argparse
import copy
import json
from pathlib import Path

import numpy as np

from cooked_contact_pose import sha
from measure_skin_contact import load_raw
from reparent_source_legs import rotate


def directions(world, names, side, heights):
    hip, knee, foot = [world[names.index(b + '_' + side)] for b in ('thigh', 'calf', 'foot')]
    upper, lower = knee[0] - hip[0], foot[0] - knee[0]
    flex = np.degrees(np.arccos(np.clip(np.sum(upper * lower, axis=1) /
        np.linalg.norm(upper, axis=1) / np.linalg.norm(lower, axis=1), -1, 1)))
    axis = foot[0] - hip[0]
    axis /= np.linalg.norm(axis, axis=1)[:, None]
    bend = upper - axis * np.sum(upper * axis, axis=1)[:, None]
    bend /= np.maximum(np.linalg.norm(bend, axis=1)[:, None], 1e-12)
    inverse = hip[1].copy()
    inverse[:, :3] *= -1
    local = rotate(inverse, bend)
    stable = np.flatnonzero(flex >= 15)
    if not len(stable):
        raise ValueError('No measured bent-leg anchors')
    result = np.zeros_like(bend)
    repairs = []
    start = None
    for i, uncertain in enumerate(np.append(flex < 15, False)):
        if uncertain and start is None:
            start = i
        if uncertain or start is None:
            continue
        left = start - 1 if start else i
        right = i if i < len(flex) else left
        dot = float(local[left] @ local[right])
        affected = [f for f in range(start, i) if heights[f] < .1]
        if affected and dot < np.cos(np.radians(45)):
            raise ValueError(f'Bent-leg anchors differ by over 45 degrees: {side} {start}:{i}')
        for frame in affected:
            t = (frame - left) / (right - left) if right != left else 0.
            target = (1 - t) * local[left] + t * local[right]
            target /= np.linalg.norm(target)
            amount = np.clip((15 - flex[frame]) / 10, 0, 1)
            amount = amount * amount * (3 - 2 * amount)
            depth = -heights[frame]
            lift = (0 if depth <= -.1 else depth if depth >= .1 else (depth + .1) ** 2 / .4)
            influence = np.clip(lift / .5, 0, 1)
            amount *= influence * influence * (3 - 2 * influence)
            if amount <= 1e-8:
                continue
            target = (1 - amount) * local[frame] + amount * target
            if np.linalg.norm(target) < .1:
                raise ValueError('Ambiguous pole interpolation')
            target /= np.linalg.norm(target)
            result[frame] = rotate(hip[1][frame], target)
        if affected:
            repairs.append(dict(start=start, end=i - 1, anchors=[left, right],
                                anchor_angle_deg=float(np.degrees(np.arccos(np.clip(dot, -1, 1)))),
                                affected_frames=affected))
        start = None
    if not np.isfinite(result).all():
        raise ValueError('Nonfinite knee direction')
    return result.tolist(), repairs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('first_trial', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    base = args.first_trial.resolve().parent
    skin_path = base / 'current-eve-skin-audit-v1/skin-audit.json'
    skin = json.loads(skin_path.read_text())
    names = [b['name'] for b in skin['bones']]
    source_plan = args.first_trial / 'plan.json'
    plan = copy.deepcopy(json.loads(source_plan.read_text()))
    plan['revision'] = 'Contact2'
    for p in (source_plan, skin_path):
        plan['inputs'][str(p.resolve())] = sha(p)
    indices = dict(bassdownlow=8, blackhole=9, comecloser=11, jangwonyoungspin=12)
    for row in plan['entries']:
        label = row['id'].split('.')[-1]
        path = (base / f'shared-full-motion-v1/raw-cook-reference/{indices[label]:02}.json.gz'
                if label != 'jangwonyoungspin' else base / 'spin-neck-ue-v1/verify-motion.json.gz')
        plan['inputs'][str(path)] = sha(path)
        world, metadata, frames = load_raw(path, names, skin)
        if frames != row['keys'] or abs(metadata['fps'] - 30) > .0001:
            raise ValueError('Source clock differs')
        row['directions'], row['pole_repairs'] = {}, {}
        for side in ('l', 'r'):
            row['directions'][side], row['pole_repairs'][side] = directions(world, names, side, row['heights'][side])
        print(label, {s: len(r) for s, r in row['pole_repairs'].items()})
    if any(sha(Path(p)) != digest for p, digest in plan['inputs'].items()):
        raise ValueError('Protected input changed')
    plan['scope'] = ('Per-foot contact with stable knee poles only below 15 degrees of original flexion. '
                     'Poles come from neighboring bent poses in thigh-local space, with smooth weight '
                     'based on flexion and required lift. All other tracks and source timing remain unchanged.')
    args.output.mkdir(parents=True)
    (args.output / 'plan.json').write_text(json.dumps(plan, indent=2) + '\n')
    (args.output / 'source-catalog.json').write_bytes((args.first_trial / 'source-catalog.json').read_bytes())


if __name__ == '__main__':
    main()

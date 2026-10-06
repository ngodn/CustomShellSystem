"""Build a pelvis-only clearance plan from a complete cooked visible-surface scan."""
import argparse
import json
from pathlib import Path

import numpy as np

from cooked_contact_pose import sha


def clearance_envelope(heights, substeps, margin=.01):
    heights = np.asarray(heights, dtype=float)
    if (heights.ndim != 1 or len(heights) < 2 or not np.isfinite(heights).all() or
            type(substeps) is not int or substeps not in (1, 2, 4) or
            (len(heights) - 1) % substeps or not np.isfinite(margin) or not 0 <= margin <= .1):
        raise ValueError('Invalid contact samples, clock or margin')
    needed = np.maximum(margin - heights, 0)
    intervals = np.maximum(needed[:-1].reshape(-1, substeps).max(axis=1), needed[substeps::substeps])
    keys = np.zeros(len(intervals) + 1)
    keys[:-1] = intervals
    keys[1:] = np.maximum(keys[1:], intervals)
    # Both endpoints bound every measured position in their interval. Expanding
    # before the positive convolution keeps that bound while easing the lift.
    radius = 2
    padded = np.pad(keys, radius, mode='edge')
    expanded = np.maximum.reduce([padded[i:i + len(keys)] for i in range(2 * radius + 1)])
    kernel = np.array([1, 2, 3, 2, 1], dtype=float) / 9
    smooth = np.convolve(np.pad(expanded, radius, mode='edge'), kernel, mode='valid')
    dense = np.interp(np.arange(len(heights)) / substeps, np.arange(len(smooth)), smooth)
    if np.min(heights + dense) < margin - 1e-9 or np.any(smooth < -1e-9):
        raise ValueError('Clearance plan does not bound the supplied surface')
    return smooth, dense


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('contact', type=Path)
    parser.add_argument('base', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    report = json.loads(args.contact.read_text())
    base = json.loads(args.base.read_text())
    if report.get('complete') is not True or len(report['results']) != 1 or not report.get('materials'):
        raise ValueError('Require one completed explicit-material scan')
    result = report['results'][0]
    if set(result['sides']) != {'surface'} or result['fps'] / base['fps'] not in (1, 2, 4):
        raise ValueError('Expected a complete surface and compatible sample clocks')
    substeps = int(result['fps'] / base['fps'])
    heights = result['sides']['surface']['frame_min_z_cm']
    if len(heights) != result['frames'] or result['frames'] != (base['frames'] - 1) * substeps + 1:
        raise ValueError('Contact and base frame counts disagree')
    old = np.asarray(base['lift_cm'], dtype=float)
    if old.shape != (base['frames'],) or not np.isfinite(old).all() or np.any(old < 0):
        raise ValueError('Invalid previous clearance')
    extra, dense = clearance_envelope(heights, substeps)
    total = old + extra
    if np.max(total) > 50:
        raise ValueError('Required clearance exceeds the native helper limit')
    inputs = dict(base['inputs'])
    for path, checksum in report['inputs'].items():
        if path in inputs and inputs[path] != checksum:
            raise ValueError('Conflicting source revisions')
        inputs[path] = checksum
    for path in (args.contact, args.base):
        inputs[str(path.resolve())] = sha(path)
    if any(sha(Path(path)) != checksum for path, checksum in inputs.items()):
        raise ValueError('Protected input changed')
    plan = dict(frames=base['frames'], fps=base['fps'], visible_materials=report['materials'],
        inputs=inputs, lift_cm=total.tolist(), added_lift_cm=extra.tolist(),
        max_lift_cm=float(total.max()), max_added_lift_cm=float(extra.max()),
        max_lift_step_cm=float(np.abs(np.diff(total)).max()),
        predicted_floor_min_cm=float(np.min(np.asarray(heights) + dense)),
        margin_cm=.01, substeps=substeps, smoothing_radius_frames=2,
        scope='Predicted pelvis-only lift from complete visible surfaces. All joint rotations remain unchanged; fresh native bake, cook and contact verification required.')
    args.output.write_text(json.dumps(plan, indent=2, allow_nan=False) + '\n')
    print({key: value for key, value in plan.items() if key.startswith(('max_', 'predicted_'))})


if __name__ == '__main__':
    main()

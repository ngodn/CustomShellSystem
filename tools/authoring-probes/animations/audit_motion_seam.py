"""Measure raw local-pose endpoints without changing or approving a clip's loop mode."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def vector(transform, key, axes):
    result = tuple(float(transform[key][axis]) for axis in axes)
    if not all(math.isfinite(value) for value in result):
        raise ValueError('Nonfinite transform')
    return result


def distance(left, right):
    return math.sqrt(sum((a - b) ** 2 for a, b in zip(left, right, strict=True)))


def quaternion(transform):
    values = vector(transform, 'Rotation', 'XYZW')
    length = math.sqrt(sum(value * value for value in values))
    if length < 1e-8:
        raise ValueError('Zero quaternion')
    return tuple(value / length for value in values)


def audit(motion):
    fps = float(motion['fps'])
    frames = motion['frames']
    if not math.isfinite(fps) or fps <= 0 or len(frames) < 3:
        raise ValueError('Need a positive sample rate and at least three frames')
    snapshots = [frame['pose']['Snapshot'] for frame in
                 (frames[0], frames[1], frames[-2], frames[-1])]
    names = snapshots[0]['BoneNames']
    if not names or len(set(names)) != len(names):
        raise ValueError('Empty or duplicate bone names')
    for snapshot in snapshots:
        if (not snapshot['bIsValid'] or snapshot['BoneNames'] != names or
                len(snapshot['LocalTransforms']) != len(names) or
                snapshot['SkeletalMeshName'] != snapshots[0]['SkeletalMeshName']):
            raise ValueError('Endpoint snapshots have incompatible skeletons')
    rows = []
    for index, name in enumerate(names):
        transforms = [snapshot['LocalTransforms'][index] for snapshot in snapshots]
        positions = [vector(t, 'Translation', 'XYZ') for t in transforms]
        rotations = [quaternion(t) for t in transforms]
        scales = [vector(t, 'Scale3D', 'XYZ') for t in transforms]
        start_velocity = tuple((b - a) * fps for a, b in
                               zip(positions[0], positions[1], strict=True))
        end_velocity = tuple((b - a) * fps for a, b in
                             zip(positions[2], positions[3], strict=True))
        dot = abs(sum(a * b for a, b in zip(rotations[0], rotations[3], strict=True)))
        rows.append(dict(bone=name,
                         position_gap_cm=distance(positions[0], positions[3]),
                         orientation_gap_degrees=math.degrees(2 * math.acos(min(1., dot))),
                         scale_gap=distance(scales[0], scales[3]),
                         translation_velocity_gap_cm_s=distance(start_velocity, end_velocity)))
    return dict(sample_rate=fps, frames=len(frames), duration_seconds=(len(frames) - 1) / fps,
                scope='Local-pose endpoints and one-sided translational velocities. '
                      'No world-space contact, angular-velocity, interpolation or loop acceptance.',
                bones=rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('motion', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = args.motion.resolve(strict=True)
    payload = source.read_bytes()
    result = audit(json.loads(payload))
    result.update(motion=str(source), sha256=hashlib.sha256(payload).hexdigest())
    if source.read_bytes() != payload:
        raise ValueError('Input changed during audit')
    with args.output.open('x') as stream:
        json.dump(result, stream, indent=2, allow_nan=False)
        stream.write('\n')


if __name__ == '__main__':
    main()

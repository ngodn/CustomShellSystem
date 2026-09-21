"""Soften an idle's head gesture without changing its stance or body proportions."""
import argparse
import copy
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
BONES = {'neck_01', 'neck_02', 'head'}


def normalized(q):
    length = math.sqrt(sum(x*x for x in q))
    if not math.isfinite(length) or abs(length-1) > .0001:
        raise ValueError('Expected a finite unit quaternion')
    return [x/length for x in q]


def soften(start, current, factor):
    a, b = normalized(start), normalized(current)
    dot = sum(x*y for x, y in zip(a, b, strict=True))
    if dot < 0:
        b, dot = [-x for x in b], -dot
    angle = math.acos(min(1., dot))
    if angle < .00001:
        return normalized([(1-factor)*x+factor*y for x, y in zip(a, b, strict=True)])
    weights = [math.sin((1-factor)*angle)/math.sin(angle),
               math.sin(factor*angle)/math.sin(angle)]
    return normalized([weights[0]*x+weights[1]*y for x, y in zip(a, b, strict=True)])


def prepare(document, factor):
    if not math.isfinite(factor) or not 0 < factor < 1:
        raise ValueError('Gesture factor must be between zero and one')
    if document['fps'] != 30 or len(document['frames']) != 211:
        raise ValueError('Expected the fitted seven-second Eve idle')
    result = copy.deepcopy(document)
    first = document['frames'][0]['pose']['Snapshot']
    names = first['BoneNames']
    if not BONES <= set(names) or len(names) != len(set(names)):
        raise ValueError('Missing or duplicate head-chain bones')
    indices = {name: names.index(name) for name in BONES}
    max_before = max_after = 0.
    for frame, original in zip(result['frames'], document['frames'], strict=True):
        snapshot = frame['pose']['Snapshot']
        if snapshot['BoneNames'] != names:
            raise ValueError('Bone order changed between frames')
        for index in indices.values():
            start = [first['LocalTransforms'][index]['Rotation'][k] for k in 'XYZW']
            value = snapshot['LocalTransforms'][index]
            current = [value['Rotation'][k] for k in 'XYZW']
            fitted = soften(start, current, factor)
            value['Rotation'] = dict(zip('XYZW', fitted, strict=True))
            for q, before in [(current, True), (fitted, False)]:
                dot = abs(sum(x*y for x, y in zip(normalized(start), normalized(q), strict=True)))
                angle = math.degrees(2*math.acos(min(1., dot)))
                if before:
                    max_before = max(max_before, angle)
                else:
                    max_after = max(max_after, angle)
        # Removing only the intended rotations must recover the original frame.
        check = copy.deepcopy(frame)
        for index in indices.values():
            check['pose']['Snapshot']['LocalTransforms'][index]['Rotation'] = \
                original['pose']['Snapshot']['LocalTransforms'][index]['Rotation']
        if check != original:
            raise ValueError('Gesture edit changed unrelated pose data')
    result['authored_rotation_bones'] = sorted(BONES)
    return result, dict(factor=factor, frames=211, changed_bones=sorted(BONES),
                       maximum_local_gesture_before_deg=max_before,
                       maximum_local_gesture_after_deg=max_after,
                       scope='Local head-chain rotation excursion from the first pose; not visual acceptance.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--factor', type=float, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT/'CustomShellSystem/work'):
        raise ValueError('Output must stay in CSS work')
    result, report = prepare(json.loads(args.input.read_text()), args.factor)
    with output.open('x') as file:
        json.dump(result, file, separators=(',', ':'))
        file.write('\n')
    print(json.dumps(report, indent=2))

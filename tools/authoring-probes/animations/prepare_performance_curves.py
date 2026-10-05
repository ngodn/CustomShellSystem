"""Preserve decoded animation curves and report gaps in a supplied facial-shape set."""
import argparse
import bisect
import copy
import hashlib
import json
import math
import re
from pathlib import Path


def evaluate(curve, seconds):
    """Evaluate a prepared linear curve, retaining its constant extrapolation."""
    if not math.isfinite(seconds):
        raise ValueError('Invalid evaluation time')
    keys = curve['FloatCurve']['Keys']
    index = bisect.bisect_right(keys, seconds, key=lambda key: key['Time'])
    if index == 0:
        return keys[0]['Value']
    if index == len(keys):
        return keys[-1]['Value']
    left, right = keys[index-1], keys[index]
    alpha = (seconds-left['Time']) / (right['Time']-left['Time'])
    return left['Value'] + (right['Value']-left['Value']) * alpha


def prepare(metadata, shape_names):
    if metadata['Type'] != 'AnimSequence':
        raise ValueError('Expected one animation sequence')
    duration = metadata['Properties']['SequenceLength']
    if type(duration) not in (int, float) or not math.isfinite(duration) or duration <= 0:
        raise ValueError('Invalid clip duration')
    names = [item['DisplayName'] for item in metadata.get('CompressedCurveNames', [])]
    curves = metadata.get('CompressedCurveData', {}).get('FloatCurves', [])
    if (len(names) != len(curves)
            or any(not isinstance(name, str) or not name for name in names)
            or len({name.casefold() for name in names}) != len(names)):
        raise ValueError('Ambiguous or mismatched curve names')
    if any(not isinstance(name, str) or not name for name in shape_names):
        raise ValueError('Invalid shape name')
    available = {name.casefold() for name in shape_names}
    summary = []
    for name, curve in zip(names, curves, strict=True):
        if curve['CurveName'] != name:
            raise ValueError('Curve names disagree with the compressed name table')
        data = curve['FloatCurve']
        keys = data['Keys']
        if not keys or not math.isfinite(data['DefaultValue']):
            raise ValueError('Empty curve or invalid default')
        if any(data[key] != 'RCCE_Constant' for key in ('PreInfinityExtrap', 'PostInfinityExtrap')):
            raise ValueError('This preparation only supports constant extrapolation')
        last = -math.inf
        for key in keys:
            if (key['InterpMode'] != 'RCIM_Linear' or key['TangentWeightMode'] != 'RCTWM_WeightedNone'
                    or any(not math.isfinite(key[field]) for field in
                           ('Time', 'Value', 'ArriveTangent', 'ArriveTangentWeight',
                            'LeaveTangent', 'LeaveTangentWeight'))
                    or key['Time'] <= last or not 0 <= key['Time'] <= duration + 1e-5):
                raise ValueError('Unsupported interpolation or invalid curve key')
            last = key['Time']
        values = [key['Value'] for key in keys]
        summary.append(dict(name=name, keys=len(keys), minimum=min(values), maximum=max(values),
                            first_seconds=keys[0]['Time'], last_seconds=keys[-1]['Time'],
                            available_shape=name.casefold() in available))
    return dict(asset=metadata['Package'], duration_seconds=duration,
                curves=copy.deepcopy(curves), summary=summary,
                missing_shapes=[row['name'] for row in summary if not row['available_shape']],
                scope='Original named keys, signed values, tangents, flags and extrapolation. '
                      'No clamping, resampling, alias inference, cook or runtime activation.')


def native_performance(metadata, shape_names, clip):
    """Convert verified linear source keys to the bounded CSS facial catalog schema."""
    if (not isinstance(clip, str) or not re.fullmatch(r'[A-Za-z0-9_.-]{1,96}', clip)
            or clip in ('original', 'off', '.', '..')):
        raise ValueError('Invalid canonical body clip identity')
    names = {}
    for name in shape_names:
        if (not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9_]{1,64}', name)
                or name.casefold() in names):
            raise ValueError('Invalid or ambiguous facial shape names')
        names[name.casefold()] = name
    source = prepare(metadata, shape_names)
    if (source['missing_shapes'] or not 1 <= len(source['curves']) <= 128
            or not 0 < source['duration_seconds'] <= 86400):
        raise ValueError('Incomplete or out-of-range facial performance')
    curves, total = [], 0
    for curve in source['curves']:
        keys = curve['FloatCurve']['Keys']
        total += len(keys)
        if not 1 <= len(keys) <= 16384 or total > 65536:
            raise ValueError('Facial key count exceeds runtime bounds')
        if any(type(key[field]) not in (int, float) for key in keys for field in ('Time', 'Value')):
            raise ValueError('Facial keys must contain numeric times and weights')
        if any(abs(key['Value']) > 4 for key in keys):
            raise ValueError('Facial weight exceeds runtime bounds')
        curves.append(dict(morph=names[curve['CurveName'].casefold()],
                           keys=[[key['Time'], key['Value']] for key in keys]))
    return dict(clip=clip, duration=source['duration_seconds'], curves=curves)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('metadata', type=Path)
    parser.add_argument('shapes', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--clip', help='Canonical body clip ID for an additional native catalog entry')
    args = parser.parse_args()
    inputs = {path.resolve(strict=True): path.read_bytes() for path in (args.metadata, args.shapes)}
    metadata = json.loads(inputs[args.metadata.resolve()])
    shapes = json.loads(inputs[args.shapes.resolve()])
    result = prepare(metadata, [shape['name'] for shape in shapes['morph_targets']])
    if args.clip:
        result['performance'] = native_performance(metadata,
            [shape['name'] for shape in shapes['morph_targets']], args.clip)
    result['inputs'] = {str(path): hashlib.sha256(data).hexdigest() for path, data in inputs.items()}
    if any(path.read_bytes() != data for path, data in inputs.items()):
        raise ValueError('Input changed during curve preparation')
    with args.output.open('x') as stream:
        json.dump(result, stream, indent=2, allow_nan=False)
        stream.write('\n')


if __name__ == '__main__':
    main()

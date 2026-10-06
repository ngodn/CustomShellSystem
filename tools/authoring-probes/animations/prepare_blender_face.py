"""Prepare an audited single-strip Blender face with a bounded interpolation error."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re


def linearize(points, duration, tolerance=1e-5):
    """Bound linear interpolation of a cubic whose time handles are uniform."""
    if (len(points) != 4 or any(len(point) != 2 for point in points)
            or any(not math.isfinite(value) for point in points for value in point)
            or not math.isfinite(duration) or duration <= 0
            or not 1e-7 <= tolerance <= 1e-3):
        raise ValueError('Invalid cubic or error bound')
    start, end = points[0][0], points[-1][0]
    if end <= start or any(abs(point[0]-(start+(end-start)*index/3)) > 1e-7
                          for index, point in enumerate(points)):
        raise ValueError('Nonuniform time handles need a different conversion')
    y = [point[1] for point in points]
    if max(abs(value) for value in y) > 4:
        raise ValueError('Facial controls exceed runtime bounds')
    # For linear segments, error <= max(abs(f'')) * step**2 / 8.
    # This cubic's second derivative is linear, so its endpoints bound it.
    derivative = 6*max(abs(y[2]-2*y[1]+y[0]), abs(y[3]-2*y[2]+y[1]))
    intervals = max(1, math.ceil(math.sqrt(derivative/(8*tolerance))))
    if intervals+1 > 16384:
        raise ValueError('Curve exceeds the runtime key budget')
    def value(t):
        return (1-t)**3*y[0]+3*(1-t)**2*t*y[1]+3*(1-t)*t*t*y[2]+t**3*y[3]
    keys = [[duration*index/intervals, value(index/intervals)] for index in range(intervals+1)]
    return keys, derivative/(8*intervals*intervals)


def prepare(inspection, audit, shape_names, clip):
    if (not re.fullmatch(r'[A-Za-z0-9_.-]{1,96}', clip) or clip in ('original','off','.','..')
            or inspection['sha256'] != audit['sha256'] or inspection['source'] != audit['source']
            or audit['autoexec_enabled'] or inspection['autoexec_enabled']):
        raise ValueError('Source identity or audit mismatch')
    meshes = [mesh for mesh in inspection['meshes'] if mesh['name'] == audit['mesh']]
    if len(meshes) != 1:
        raise ValueError('Ambiguous source face')
    data = meshes[0]['shape_animation']
    if (not data or data['action'] or data['drivers'] or not data['use_nla']
            or any(meshes[0]['shape_values'].values())):
        raise ValueError('Expected a neutral base face with NLA only')
    tracks = [track for track in data['nla'] if not track['mute']]
    if len(tracks) != 1 or tracks[0]['solo'] or len(tracks[0]['strips']) != 1:
        raise ValueError('Expected one facial NLA strip')
    strip = tracks[0]['strips'][0]
    if (strip['type'] != 'CLIP' or strip['mute'] or strip['blend_type'] != 'REPLACE'
            or strip['influence'] != 1 or strip['scale'] != 1 or strip['repeat'] != 1
            or strip['extrapolation'] != 'HOLD' or strip['animated_time'] or strip['animated_influence']
            or strip['scene_range'] != audit['scene_frames'] or strip['action_range'] != audit['scene_frames']):
        raise ValueError('Facial NLA timing or blending requires additional handling')
    scenes = [scene for scene in inspection['scenes'] if scene['fps']/scene['fps_base'] == audit['scene_rate']]
    start, end = audit['scene_frames']
    if len(scenes) != 1 or abs((end-start)/audit['scene_rate']-audit['duration']) > 1e-7:
        raise ValueError('Source face timing differs from the body')
    if len(audit['active_curves']) != 1:
        raise ValueError('This source adapter expects one active facial curve')
    curve = audit['active_curves'][0]
    match = re.fullmatch(r'key_blocks\["([A-Za-z0-9_]+)"\]\.value', curve['path'])
    if (not match or match[1] not in shape_names or curve['action'] != strip['action']
            or curve['slot'] != strip['action_slot'] or curve['index'] != 0
            or curve['mute'] or curve['modifiers'] or curve['extrapolation'] != 'CONSTANT'
            or len(curve['keys']) != 2 or any(key['interpolation'] != 'BEZIER' for key in curve['keys'])):
        raise ValueError('Unsupported active facial curve')
    left, right = curve['keys']
    if left['co'][0] != start or right['co'][0] != end:
        raise ValueError('Curve does not cover the body interval')
    points = [left['co'],left['right'],right['left'],right['co']]
    keys, bound = linearize(points,audit['duration'])
    name = match[1]
    index = audit['names'].index(name)
    samples = audit['samples']
    if (len(samples) < 3 or samples[0]['seconds'] != 0 or samples[-1]['seconds'] != audit['duration']
            or any(a['seconds'] >= b['seconds'] for a,b in zip(samples,samples[1:]))):
        raise ValueError('Incomplete evaluated source interval')
    worst = 0.
    for sample in samples:
        if (not math.isfinite(sample['seconds']) or len(sample['values']) != len(audit['names'])
                or any(not math.isfinite(value) for value in sample['values'])):
            raise ValueError('Malformed evaluated source sample')
        t = sample['seconds']/audit['duration']
        y = [point[1] for point in points]
        expected = (1-t)**3*y[0]+3*(1-t)**2*t*y[1]+3*(1-t)*t*t*y[2]+t**3*y[3]
        worst = max(worst,abs(expected-sample['values'][index]))
        if any(value != 0 for i,value in enumerate(sample['values']) if i != index):
            raise ValueError('An additional facial control affects this source')
    if worst > 2e-6:
        raise ValueError('Source Bezier and evaluated scene disagree')
    return dict(performance=dict(clip=clip,duration=audit['duration'],curves=[dict(morph=name,keys=keys)]),
                interpolation_error_bound=bound, scene_comparison_error=worst,
                scope='Uniform-time source Bezier converted to bounded linear segments. '
                      'Original source and controls retained; not installed or live accepted.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inspection',type=Path)
    parser.add_argument('audit',type=Path)
    parser.add_argument('shapes',type=Path)
    parser.add_argument('output',type=Path)
    parser.add_argument('--clip',required=True)
    args = parser.parse_args()
    paths = [path.resolve(strict=True) for path in (args.inspection,args.audit,args.shapes)]
    inputs = {path:path.read_bytes() for path in paths}
    inspection,audit,shapes = [json.loads(inputs[path]) for path in paths]
    result = prepare(inspection,audit,[shape['name'] for shape in shapes['morph_targets']],args.clip)
    for path,expected in ((Path(audit['source']),audit['sha256']),
                          (Path(audit['body']),audit['body_sha256'])):
        content = path.read_bytes()
        if hashlib.sha256(content).hexdigest() != expected:
            raise ValueError('An original audit input changed')
        inputs[path.resolve()] = content
    if any(path.read_bytes() != content for path,content in inputs.items()):
        raise ValueError('Input changed during conversion')
    result['inputs'] = {str(path):hashlib.sha256(content).hexdigest() for path,content in inputs.items()}
    with args.output.open('x') as stream:
        json.dump(result,stream,indent=2,allow_nan=False)
        stream.write('\n')
    print(json.dumps({key:value for key,value in result.items() if key not in ('performance','inputs')}))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Check actual menu-plane corners against the live viewport. Python 3.14.

Read-only. Perspective-project the mesh corners and require their convex
polygon to contain every screen corner. This is independent of the native
ray/plane sizing calculation. Open the CSS page before running.
"""
import argparse
import json
import math
from pathlib import Path

from css import ROOT, atomic
from css_capture import command
from css_live_snapshot import Probe
from check_inventory_camera import checked_get, checked_call


def xyz(value):
    return [value[key] for key in ('X', 'Y', 'Z')]


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def contains_screen(polygon):
    def cross(a, b, p):
        return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
    return all(all(v >= -1e-7 for v in signs) or all(v <= 1e-7 for v in signs)
               for point in ((-1, -1), (1, -1), (1, 1), (-1, 1))
               for signs in [[cross(polygon[i], polygon[(i+1) % 4], point)
                              for i in range(4)]])


def inspect(probe, inventory):
    if not inventory['css']['active']:
        raise RuntimeError('Open the CSS page first')
    display = probe.send('find', path=inventory['display']['path'])
    child = probe.send('find', path=inventory['camera_child'])
    camera = checked_get(probe, child, 'CameraComponent')

    def vector(target, function):
        return xyz(checked_call(probe, target, function)['ReturnValue'])

    position = vector(camera, 'K2_GetComponentLocation')
    forward = vector(camera, 'GetForwardVector')
    right = vector(camera, 'GetRightVector')
    up = vector(camera, 'GetUpVector')
    fov = checked_call(probe, camera, 'GetHorizontalFieldOfView')['ReturnValue']
    width, height = inventory['css']['layout_size']
    aspect = width/height
    tangent = math.tan(math.radians(fov/2))
    planes = {}
    for name in ('BG_Front', 'BG_Back'):
        component = checked_get(probe, display, name)
        center = vector(component, 'K2_GetComponentLocation')
        scale = vector(component, 'K2_GetComponentScale')
        axis_x = vector(component, 'GetForwardVector')
        axis_y = vector(component, 'GetRightVector')
        bounds = checked_call(probe, component, 'GetLocalBounds')
        polygon = []
        for x, y in (('Min', 'Min'), ('Max', 'Min'), ('Max', 'Max'), ('Min', 'Max')):
            corner = [center[i] + axis_x[i]*bounds[x]['X']*scale[0]
                      + axis_y[i]*bounds[y]['Y']*scale[1] - position[i] for i in range(3)]
            depth = dot(corner, forward)
            if depth <= 0:
                raise RuntimeError('Backdrop corner is behind the camera')
            polygon.append([dot(corner, right)/(depth*tangent),
                            dot(corner, up)/(depth*tangent/aspect)])
        planes[name] = dict(component=component, center=center, scale=scale,
                            screen_corners=polygon, covers_viewport=contains_screen(polygon),
                            relative_location=checked_get(probe, component, 'RelativeLocation'),
                            relative_scale=checked_get(probe, component, 'RelativeScale3D'))
    after = command('inventory_inspect')
    stable = (after['css']['active'] and after['display'].get('path') == inventory['display']['path']
              and all(abs(after['css'][key]-inventory['css'][key]) < 1e-6
                      for key in ('zoom', 'pan', 'frame'))
              and after['css']['layout_size'] == inventory['css']['layout_size'])
    return dict(camera=position, fov=fov, aspect=aspect, view=inventory['css'], planes=planes,
                view_after=after['css'], coherent=stable,
                passed=stable and all(row['covers_viewport'] for row in planes.values()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT/'work') or output.exists():
        parser.error('Use a new output under CustomShellSystem/work')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.with_suffix('.requests.jsonl').open('x') as log:
        result = inspect(Probe(log), command('inventory_inspect'))
    atomic(output, result)
    print(json.dumps(dict(output=str(output), passed=result['passed'],
                         planes={k:v['covers_viewport'] for k,v in result['planes'].items()})))
    raise SystemExit(0 if result['passed'] else 1)


if __name__ == '__main__':
    main()

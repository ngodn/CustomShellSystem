#!/usr/bin/env python3
"""Check the active CSS preview against its actual camera output. Python 3.14.

Read-only live check. Open the CSS page first. A diagnostic ownership flag or
view-target actor alone cannot establish which camera pose is being rendered.
"""
import argparse
import json
import math
from pathlib import Path

from css import ROOT, atomic, processes
from css_capture import command
from css_live_snapshot import Probe


def checked_get(probe, target, name):
    props = probe.send('properties', target=target, inherited=True)
    if not any(prop['name'] == name for prop in props):
        raise RuntimeError(f'Missing reflected property: {name}')
    return probe.send('get', target=target, property=name)


def checked_call(probe, target, name, **args):
    probe.send('describe', target=target, function=name)
    return probe.send('call', target=target, function=name, args=args)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'work') or output.exists():
        parser.error('Use a new output inside CustomShellSystem/work')
    if len(processes()) != 1:
        parser.error('Exactly one running game is required')
    output.parent.mkdir(parents=True, exist_ok=True)
    inventory = command('inventory_inspect')
    if not inventory['css']['active'] or not inventory.get('camera_child'):
        raise RuntimeError('Open the active CSS character preview first')
    with output.with_suffix('.requests.jsonl').open('x') as log:
        p = Probe(log)
        player = p.send('player')
        pc = player['controller']
        manager = checked_get(p, pc, 'PlayerCameraManager')
        child = p.send('find', path=inventory['camera_child'])
        component = checked_get(p, child, 'CameraComponent')
        display = p.send('find', path=inventory['display']['path'])
        selected = checked_call(p, display, 'GetCameraActor')
        actual = checked_call(p, manager, 'GetCameraLocation')['ReturnValue']
        rotation = checked_call(p, manager, 'GetCameraRotation')['ReturnValue']
        expected = checked_call(p, component, 'K2_GetComponentLocation')['ReturnValue']
        expected_rotation = checked_call(p, component, 'K2_GetComponentRotation')['ReturnValue']
        fov = checked_call(p, manager, 'GetFOVAngle')['ReturnValue']
        expected_fov = checked_call(p, component, 'GetHorizontalFieldOfView')['ReturnValue']
        after = p.send('player')
        if after != player:
            raise RuntimeError('Player changed during camera inspection')
    distance = math.dist([actual[k] for k in ('X', 'Y', 'Z')],
                         [expected[k] for k in ('X', 'Y', 'Z')])
    angle = max(abs((rotation[k] - expected_rotation[k] + 180) % 360 - 180)
                for k in ('Pitch', 'Yaw', 'Roll'))
    failures = []
    if distance > 1:
        failures.append(f'Rendered camera differs from preview by {distance:.3f} cm')
    if angle > 1:
        failures.append(f'Rendered camera rotation differs by {angle:.3f} degrees')
    if abs(fov - expected_fov) > .05:
        failures.append(f'Rendered FOV differs from preview by {abs(fov - expected_fov):.3f} degrees')
    result = dict(player=player, inventory=inventory, selected_camera=selected,
                  actual=actual, expected=expected, rotation=rotation,
                  expected_rotation=expected_rotation, distance_cm=distance,
                  fov=fov, expected_fov=expected_fov,
                  max_euler_error_degrees=angle, failures=failures)
    atomic(output, result)
    print(json.dumps(dict(output=str(output), failures=failures,
                          selected_camera=selected, distance_cm=distance,
                          max_euler_error_degrees=angle)))
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Measure enabled/disabled hair during preview orbit. Python 3.14.

Parent-bone transforms remove the character's head and whole-body movement.
Seven chain roots are sampled serially, not as an atomic animation frame.
"""
import argparse
import itertools
import json
import math
from pathlib import Path
import time

from css import ROOT, atomic
from css_capture import MOD, command
from css_live_snapshot import Probe
from check_live_hair_controls import snapshot


BONES = ['CSS_Hair_Ponytail_01'] + [f'CSS_Hair_{part}_{side}_01'
    for part in ('Bangs', 'BangsOuter', 'Side') for side in ('L', 'R')]


def angle(a, b):
    qa = [a['Rotation'][k] for k in ('X', 'Y', 'Z', 'W')]
    qb = [b['Rotation'][k] for k in ('X', 'Y', 'Z', 'W')]
    dot = abs(sum(x * y for x, y in zip(qa, qb))) / (math.hypot(*qa) * math.hypot(*qb))
    return math.degrees(2 * math.acos(min(1, dot)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'work') or output.exists():
        parser.error('Use a new output directory inside CustomShellSystem/work')
    output.mkdir(parents=True)
    state = json.loads((MOD / 'state/state.json').read_text())
    atomic(output / 'state-before.json', state)
    status = command('status')
    selection = state['selections'][status['shell']]
    assert selection['outfit'] == 'eins0fx.seduxtress'
    original = selection['customize']['values'].get('hair_motion')
    with (output / 'requests.jsonl').open('x') as log:
        p = Probe(log)
        before = snapshot(p)
        mesh = before['preview']['mesh']
        view = before['inventory']['css']
        original_view = [view[k] for k in ('yaw', 'zoom', 'pan', 'frame')]
        signature = p.send('describe', target=mesh, function='GetSocketTransform')
        space = next(x['value'] for x in signature['TransformSpace']['enum']
                     if x['name'] == 'RTS_ParentBoneSpace')
        p.send('describe', target=mesh, function='DoesSocketExist')
        for bone in BONES:
            assert p.call(mesh, 'DoesSocketExist', InSocketName=bone), bone
        cases = {}
        try:
            command('reset_control', control='hair_motion')
            for enabled in (False, True):
                command('control', control='hair_motion', channel=3, value=int(enabled))
                time.sleep(2)
                case = dict(samples=[])
                name = 'enabled' if enabled else 'disabled'
                cases[name] = case
                command('inventory_capture_motion', view=[original_view[0] + 160, *original_view[1:]], seconds=5)
                started = time.monotonic()
                returning = False
                while time.monotonic() - started < 12:
                    if not returning and time.monotonic() - started >= 5:
                        command('inventory_capture_motion', view=original_view, seconds=5)
                        returning = True
                    row = dict(seconds=time.monotonic() - started, bones={})
                    for bone in BONES:
                        row['bones'][bone] = p.call(mesh, 'GetSocketTransform',
                            InSocketName=bone, TransformSpace=space)
                    case['samples'].append(row)
                    atomic(output / (name + '.json'), case)
                case['max_rotation_span_degrees'] = {
                    bone: max(angle(a['bones'][bone], b['bones'][bone])
                              for a, b in itertools.combinations(case['samples'], 2))
                    for bone in BONES}
                atomic(output / (name + '.json'), case)
        finally:
            if original is None:
                command('reset_control', control='hair_motion')
            else:
                for channel, value in enumerate(original):
                    command('control', control='hair_motion', channel=channel, value=value)
            now = command('inventory_inspect')['css']
            command('inventory_test_motion', movement=[v - now[k] for v, k in
                zip(original_view, ('yaw', 'zoom', 'pan', 'frame'))])
            time.sleep(1.2)
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(output / 'state-after.json', after)
            assert after == state, 'Saved settings were not restored'
        assert p.send('player') == before['player_identity']
        disabled = cases['disabled']['max_rotation_span_degrees']
        enabled = cases['enabled']['max_rotation_span_degrees']
        failures = []
        if max(disabled.values()) > .05:
            failures.append('Disabled hair roots rotate relative to their parents')
        if enabled[BONES[0]] < 1:
            failures.append('No meaningful enabled ponytail root motion measured')
        result = dict(disabled=disabled, enabled=enabled, failures=failures,
                      state_restored=True, sample_counts={k: len(v['samples']) for k,v in cases.items()},
                      scope='Seven root local rotations during preview orbit; no contact or gameplay acceptance')
        atomic(output / 'result.json', result)
        print(json.dumps(result))
        raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()

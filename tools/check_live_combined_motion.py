#!/usr/bin/env python3
"""Measure V42 body and hair output during controlled preview turns. Python 3.14."""
import argparse
import itertools
import json
import math
from pathlib import Path
import time

from css import ROOT, atomic, processes
from css_capture import MOD, command
from css_live_snapshot import Probe
from check_live_body_controls import CONTROLS, snapshot
from check_live_hair_motion import BONES as HAIR_BONES, angle

BODY_BONES = ['brust001', 'brust002', 'butt001', 'butt002',
              'thigh_twist_02_l', 'thigh_twist_02_r', 'belly']
BONES = BODY_BONES + HAIR_BONES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'work') or output.exists() or len(processes()) != 1:
        parser.error('Use a new workspace output and one running game')
    output.mkdir(parents=True)
    state = json.loads((MOD / 'state/state.json').read_text())
    atomic(output / 'state-before.json', state)
    selection = state['selections'][command('status')['shell']]
    assert selection['outfit'] == 'eins0fx.seduxtress'
    touched = CONTROLS + ['hair_motion']
    originals = {name: selection['customize']['values'].get(name) for name in touched}
    with (output / 'requests.jsonl').open('x') as log:
        p = Probe(log)
        before = snapshot(p)
        atomic(output / 'initial.json', before)
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
            for control in touched:
                command('reset_control', control=control)
            for name, body, hair in [('all-off', False, False),
                                     ('body-off-hair-on', False, True),
                                     ('both-on', True, True)]:
                for control in CONTROLS:
                    command('control', control=control, channel=3, value=int(body))
                command('control', control='hair_motion', channel=3, value=int(hair))
                time.sleep(2)
                case = dict(samples=[], body_enabled=body, hair_enabled=hair)
                cases[name] = case
                command('inventory_capture_motion',
                        view=[original_view[0] + 160, *original_view[1:]], seconds=6)
                started = time.monotonic()
                returning = False
                while time.monotonic() - started < 15:
                    if not returning and time.monotonic() - started >= 6:
                        command('inventory_capture_motion', view=original_view, seconds=6)
                        returning = True
                    row = dict(seconds=time.monotonic() - started, bones={})
                    for bone in BONES:
                        transform = p.call(mesh, 'GetSocketTransform',
                                           InSocketName=bone, TransformSpace=space)
                        if not all(math.isfinite(v) for member in transform.values()
                                   if isinstance(member, dict) for v in member.values()):
                            raise AssertionError('Nonfinite bone transform: ' + bone)
                        row['bones'][bone] = transform
                    case['samples'].append(row)
                    atomic(output / (name + '.json'), case)
                assert len(case['samples']) >= 3, 'Insufficient motion samples'
                case['max_rotation_span_degrees'] = {
                    bone: max(angle(a['bones'][bone], b['bones'][bone])
                              for a, b in itertools.combinations(case['samples'], 2))
                    for bone in BONES}
                atomic(output / (name + '.json'), case)
        finally:
            for control, original in originals.items():
                if original is None:
                    command('reset_control', control=control)
                else:
                    for channel, value in enumerate(original):
                        command('control', control=control, channel=channel, value=value)
            now = command('inventory_inspect')['css']
            command('inventory_test_motion', movement=[v - now[k] for v, k in
                    zip(original_view, ('yaw', 'zoom', 'pan', 'frame'))])
            time.sleep(1.2)
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(output / 'state-after.json', after)
            assert after == state, 'Saved settings were not restored'
        assert p.send('player') == before['player_identity']
        spans = {k: v['max_rotation_span_degrees'] for k, v in cases.items()}
        failures = []
        if max(spans['all-off'][bone] for bone in HAIR_BONES) > .05:
            failures.append('Disabled hair has local root motion')
        for name in ('body-off-hair-on', 'both-on'):
            if spans[name][HAIR_BONES[0]] < 1:
                failures.append(name + ': enabled ponytail has no meaningful local motion')
        result = dict(passed=not failures, failures=failures, spans=spans,
                      sample_counts={k: len(v['samples']) for k, v in cases.items()},
                      state_restored=True,
                      scope='Serial local bone spans during preview turns. Body spans require interpretation against animated base pose. No contact, gameplay or performance acceptance.')
        atomic(output / 'result.json', result)
        print(json.dumps(result))
        raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()

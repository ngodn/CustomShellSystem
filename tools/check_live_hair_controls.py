#!/usr/bin/env python3
"""Check V37 hair controls in the live player and preview. Python 3.14.

Requires the V40 SeduXtress outfit and an open CSS preview. Uses normal CSS
commands, backs up settings, and restores the original override and profiles.
Optional clips capture only the specified game window without changing focus.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

from css import ROOT, atomic, processes
import css_capture
from css_capture import MOD, command
from css_live_snapshot import Probe
from check_inventory_camera import checked_call, checked_get


FIELDS = ('CSSStiffness', 'CSSDamping', 'CSSGravity', 'CSSEnabled',
          'CSSResetEpoch', 'CSSUpdateSerial')


def snapshot(probe):
    player = probe.send('player')
    inventory = command('inventory_inspect')
    if not player.get('pawn') or not inventory['css']['active']:
        raise RuntimeError('An active player and CSS preview are required')
    preview = probe.send('find', path=inventory['display_character']['path'])
    result = dict(player_identity=player, inventory=inventory)
    for label, actor in (('player', player['pawn']), ('preview', preview)):
        mesh = checked_get(probe, actor, 'Mesh')
        anim = checked_call(probe, mesh, 'GetPostProcessInstance')['ReturnValue']
        if not anim or 'ABP_SeduXtress_HairV37' not in anim['class']:
            raise RuntimeError(f'{label} does not use the production hair graph')
        props = probe.send('properties', target=anim, inherited=True)
        if not set(FIELDS).issubset({p['name'] for p in props}):
            raise RuntimeError('The production hair input contract is missing')
        values = {name: probe.send('get', target=anim, property=name) for name in FIELDS}
        result[label] = dict(mesh=mesh, anim=anim, values=values)
    if probe.send('player') != player:
        raise RuntimeError('Player changed during the check')
    return result


def assert_values(sample, expected):
    stiffness, damping, gravity, enabled = expected
    for owner in ('player', 'preview'):
        actual = sample[owner]['values']
        assert abs(actual['CSSStiffness'] - stiffness) < .0001, (owner, actual)
        assert abs(actual['CSSDamping'] - damping) < .0001, (owner, actual)
        assert abs(actual['CSSGravity']['Z'] + 980 * gravity) < .0001, (owner, actual)
        assert actual['CSSGravity']['X'] == actual['CSSGravity']['Y'] == 0
        assert actual['CSSEnabled'] == bool(enabled), (owner, actual)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--window-id', help='Optional Xwayland game window for two orbit clips')
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'work') or output.exists():
        parser.error('Use a new directory inside CustomShellSystem/work')
    if len(processes()) != 1:
        parser.error('Exactly one game is required')
    before_bytes = (MOD / 'state/state.json').read_bytes()
    before = json.loads(before_bytes)
    status = command('status')
    selected = before['selections'][status['shell']]
    if (selected['outfit'], selected['variant']) != ('eins0fx.seduxtress', 'black_pearl'):
        parser.error('Wear the V40 Black Pearl outfit first')
    original = selected['customize']['values'].get('hair_motion')
    slot = 'css-hair-check-' + str(time.time_ns())
    if len(before['presets']) >= 64:
        parser.error('No free temporary profile slot')
    output.mkdir(parents=True)
    (output / 'state-before.json').write_bytes(before_bytes)
    css_capture.MEDIA = output
    saved = False
    results = []
    with (output / 'requests.jsonl').open('x') as log:
        probe = Probe(log)

        def send(action, **fields):
            log.write(json.dumps(dict(action=action, **fields)) + '\n')
            log.flush()
            reply = command(action, **fields)
            log.write(json.dumps(reply) + '\n')
            log.flush()
            return reply

        def setting(channel, value):
            send('control', control='hair_motion', channel=channel, value=value)

        def check(name, expected):
            time.sleep(.8)
            sample = snapshot(probe)
            atomic(output / (name + '.json'), sample)
            assert sample['player_identity'] == initial['player_identity'], 'Player changed'
            assert_values(sample, expected)
            results.append(dict(name=name, expected=expected, passed=True))
            return sample

        def clip(name):
            if args.window_id:
                subprocess.run([sys.executable, str(ROOT / 'tools/css_window_clip.py'),
                                '--window-id', args.window_id, '--output', str(output / (name + '.mp4')),
                                '--seconds', '12', '--orbit'], check=True)

        initial = snapshot(probe)
        atomic(output / 'initial.json', initial)
        view = initial['inventory']['css']
        try:
            send('reset_control', control='hair_motion')
            check('defaults', [150, 18, 0, 1])
            setting(0, 250)
            check('stiffness', [250, 18, 0, 1])
            setting(1, 24)
            check('damping', [250, 24, 0, 1])
            setting(2, .2)
            check('gravity', [250, 24, .2, 1])
            setting(3, 0)
            check('disabled', [250, 24, .2, 0])
            send('inventory_capture_row', section=1, row=17, offset=1000)
            css_capture.shot('tuned-disabled')
            # Saving a disabled, non-default four-channel value catches loss of
            # the fourth channel as well as a profile that only updates the UI.
            send('save_profile', name=slot)
            saved = True
            send('reset_control', control='hair_motion')
            check('reset', [150, 18, 0, 1])
            send('load_profile', name=slot)
            check('profile-restored', [250, 24, .2, 0])
            css_capture.shot('profile-restored')
            send('reset_control', control='hair_motion')
            check('enabled-orbit', [150, 18, 0, 1])
            clip('motion-enabled')
            setting(3, 0)
            check('disabled-orbit', [150, 18, 0, 0])
            clip('motion-disabled')
            setting(3, 1)
            check('reenabled', [150, 18, 0, 1])
            # Changing Inventory tabs releases and re-acquires the preview.
            # The game may recreate its instance; never reuse graph handles.
            send('inventory_select', index=0)
            time.sleep(.8)
            send('inventory_select', index=1)
            check('tab-reentry', [150, 18, 0, 1])
        finally:
            # Use the public settings path so in-memory and persisted values
            # agree. Never overwrite the live state file behind the core.
            if original is None:
                send('reset_control', control='hair_motion')
            else:
                for channel, value in enumerate(original):
                    setting(channel, value)
            if saved:
                send('delete_profile', name=slot)
            send('inventory_capture_row', section=view['section'], row=view['row'], offset=1000)
            now = command('inventory_inspect')['css']
            send('inventory_test_motion', movement=[view[k] - now[k] for k in ('yaw', 'zoom', 'pan', 'frame')])
            time.sleep(1.2)
            after_bytes = (MOD / 'state/state.json').read_bytes()
            (output / 'state-after.json').write_bytes(after_bytes)
            assert json.loads(after_bytes) == before, 'Saved settings changed after restoration'
        restored = snapshot(probe)
        atomic(output / 'restored.json', restored)
        assert_values(restored, original or [150, 18, 0, 1])
        css_capture.shot('restored')
    atomic(output / 'result.json', dict(passed=True, cases=results, state_restored=True,
                                       media_review_pending=bool(args.window_id),
                                       limits='No travel/death, mouse/keyboard input, contact or FPS acceptance'))
    print(json.dumps(dict(passed=True, checks=len(results), output=str(output))))


if __name__ == '__main__':
    main()

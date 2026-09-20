#!/usr/bin/env python3
"""Live paused-camera ownership regression. Python 3.14.

Run with a safe, idle character. Opens/closes Inventory and moves its preview
camera only. Restores the controller's original paused-update flag on exit.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

from css import ROOT, atomic
from css_capture import command
from css_live_snapshot import Probe
from check_inventory_camera import checked_get, checked_call


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'work') or output.exists():
        parser.error('Use a new directory inside CustomShellSystem/work')
    output.mkdir(parents=True)
    with (output / 'requests.jsonl').open('x') as log:
        p = Probe(log)
        player = p.send('player')
        if not player['pawn']:
            raise RuntimeError('No playable character')
        pc = player['controller']
        handler = checked_get(p, pc, 'User Interface Handler Component')
        checked_call(p, handler, 'HandleGameMenu', SubTabIndex=0, AllowClose=False)
        time.sleep(.6)
        game = checked_get(p, handler, 'WBP_Menu_Game')
        main = checked_get(p, game, 'WBP_Menu_Main')

        def close():
            if p.send('get', target=main, property='bOpen'):
                checked_call(p, handler, 'HandleGameMenu', SubTabIndex=0, AllowClose=True)
            time.sleep(.6)
            assert not p.send('get', target=main, property='bOpen'), 'Inventory stayed open'

        close()
        original = checked_get(p, pc, 'bShouldPerformFullTickWhenPaused')
        atomic(output / 'before.json', dict(player=player, original=original))
        results = []
        try:
            for before in (False, True):
                p.send('set', target=pc, property='bShouldPerformFullTickWhenPaused', value=before)
                checked_call(p, handler, 'HandleGameMenu', SubTabIndex=0, AllowClose=False)
                command('inventory_select', index=1)
                time.sleep(.6)
                assert command('inventory_inspect')['css']['active']
                assert p.send('get', target=pc, property='bShouldPerformFullTickWhenPaused')
                name = 'original-true' if before else 'original-false'
                for phase, movement in [('entry', None), ('pan', [0, 0, 12, 8]), ('zoom', [0, .25, 0, 0])]:
                    if movement:
                        command('inventory_test_motion', movement=movement)
                        time.sleep(.6)
                    subprocess.run([sys.executable, str(ROOT / 'tools/check_inventory_camera.py'),
                                    '--output', str(output / f'{name}-{phase}.json')], check=True)
                entry = json.loads((output / f'{name}-entry.json').read_text())
                # A tab change releases CSS orbit controls, but Inventory still
                # needs camera updates. Re-entry must not capture our true flag.
                command('inventory_select', index=0)
                time.sleep(.6)
                assert not command('inventory_inspect')['css']['active']
                assert p.send('get', target=pc, property='bShouldPerformFullTickWhenPaused')
                manager = checked_get(p, pc, 'PlayerCameraManager')
                instance = checked_get(p, manager, 'ActiveCameraInstance')
                state = checked_get(p, instance, 'CameraState')
                target = checked_call(p, state, 'GetCameraTargetActor')['ReturnValue']
                display = p.send('find', path=entry['inventory']['display']['path'])
                assert target == display, 'CSS camera target was not restored'
                restored_fov = checked_call(p, manager, 'GetFOVAngle')['ReturnValue']
                restored_location = checked_call(p, manager, 'GetCameraLocation')['ReturnValue']
                assert abs(restored_fov - entry['fov']) < .05, 'Zoom leaked to Inventory'
                assert restored_location == entry['actual'], 'Pan leaked to Inventory'
                command('inventory_select', index=1)
                time.sleep(.6)
                assert command('inventory_inspect')['css']['active']
                close()
                actual = p.send('get', target=pc, property='bShouldPerformFullTickWhenPaused')
                assert actual == before, (before, actual)
                results.append(dict(original=before, restored=actual, pan=True, zoom=True,
                                    tab_reentry=True, camera_restored=True))
        finally:
            try:
                close()
            finally:
                if p.send('valid', target=pc):
                    p.send('set', target=pc, property='bShouldPerformFullTickWhenPaused', value=original)
        assert p.send('player') == player, 'Player changed during lifecycle test'
        atomic(output / 'result.json', dict(passed=True, cases=results, original=original))
        print(json.dumps(dict(passed=True, cases=results, output=str(output))))


if __name__ == '__main__':
    main()

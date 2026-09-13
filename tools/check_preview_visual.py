#!/usr/bin/env python3
"""Compare live player and wardrobe visual state. Python 3.14."""
import json
import time
from css import GAME, ROOT, atomic, processes, wait_json


def main():
    if not processes():
        raise SystemExit('Load the game before running this live check.')
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    rid = str(time.time_ns())
    atomic(mod / 'request.json', {'id': rid, 'action': 'inspect'})
    result = wait_json(mod / 'runtime/inspection.json', lambda j: j.get('id') == rid)
    failures = []
    player, preview = result.get('gameplay_visual'), result.get('preview_visual')
    if not player or not preview:
        failures.append('Open the wardrobe to compare its preview with the player.')
    else:
        for key in ('effective', 'CustomPrimitiveDataInternal', 'lighting_channels'):
            if player[key] != preview[key]:
                failures.append(f'Player/preview mismatch: {key}')
        result['foot_height_difference_cm'] = {
            name: preview['sockets'][name][2] - player['sockets'][name][2]
            for name in ('foot_l', 'foot_r', 'ball_l', 'ball_r')
        }
        # Different idle poses can move individual feet. Record the measurements
        # without claiming that socket height alone measures the visible sole.
    output = ROOT / 'work/preview-visual-comparison.json'
    output.write_text(json.dumps({'failures': failures, 'inspection': result}, indent=2) + '\n')
    print('\n'.join(failures) if failures else 'Player and preview visual state match.')
    print(output)
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()

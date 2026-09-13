#!/usr/bin/env python3
"""Compare live CSS and inventory-preview materials. Python 3.14."""
import json
import time
from css import GAME, ROOT, atomic, processes, wait_json


def main():
    if not processes():
        raise SystemExit('Load the game and open its character menu first.')
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    rid = str(time.time_ns())
    atomic(mod / 'request.json', {'id': rid, 'action': 'inspect'})
    result = wait_json(mod / 'runtime/inspection.json', lambda j: j.get('id') == rid)
    failures = []
    player, preview = result.get('gameplay_visual'), result.get('menu_visual')
    if not player or not preview:
        failures.append('Open the game character/inventory menu to inspect its preview.')
    elif player['effective'] != preview['effective']:
        failures.append('Inventory preview material slots differ from the CSS outfit.')
    output = ROOT / 'work/menu-preview-comparison.json'
    output.write_text(json.dumps({'failures': failures, 'inspection': result}, indent=2) + '\n')
    print('\n'.join(failures) if failures else 'Inventory preview and CSS outfit materials match.')
    print(output)
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()

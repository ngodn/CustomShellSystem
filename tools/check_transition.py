#!/usr/bin/env python3
"""Record CSS player/menu state through a manually played transition. Python 3.14."""
import argparse
import json
import time
from pathlib import Path
from css import GAME, atomic, processes, wait_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=180)
    parser.add_argument('--output', type=Path, default=Path('work/beacon-transition.jsonl'))
    args = parser.parse_args()
    if not 1 <= args.seconds <= 1800:
        parser.error('--seconds must be 1..1800')
    if not processes():
        parser.error('Load the game first.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    deadline = time.monotonic() + args.seconds
    with args.output.open('x') as output:
        while time.monotonic() < deadline:
            rid = str(time.time_ns())
            atomic(mod / 'request.json', {'action': 'inspect_transition', 'id': rid})
            try:
                result = wait_json(mod / 'runtime/transition.json', lambda j: j.get('id') == rid, timeout=3)
            except TimeoutError:
                # A blocked engine tick during map loading is an observation.
                result = {'id': rid, 'waiting_for_engine': True}
            output.write(json.dumps(result) + '\n')
            output.flush()
            time.sleep(.5)
    print(args.output)


if __name__ == '__main__':
    main()

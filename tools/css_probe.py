#!/usr/bin/env python3
"""Ask the running CSS core about the live game. Development only, never packaged.

CSSX is the bridge other people build extensions on. This points the same reflected
bridge at CSS's own appearance state, so its behaviour can be checked against what the
engine actually holds instead of against an offline model.

  css_probe.py '{"op":"player"}'
  css_probe.py '{"op":"get","target":<handle>,"property":"Mesh"}'
  css_probe.py '{"op":"describe","target":<handle>,"function":"GetClosestPointOnPhysicsAsset"}'
  css_probe.py seal --lift 3.0 --max-push 3.5       # retune the live correction

Python 3.14.
"""
import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from css import GAME, atomic, wait_json

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


def send(action, extra, result_file, timeout=20):
    rid = str(time.time_ns())
    atomic(MOD / 'request.json', {'id': rid, 'action': action, **extra})
    if result_file is None:
        time.sleep(1.0)
        return None
    return wait_json(MOD / 'runtime' / result_file, lambda j: j.get('id') == rid, timeout=timeout)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('request', help='a bridge request as JSON, or the word "seal"')
    parser.add_argument('--lift', type=float, default=-1, help='seal: fixed cm along the recorded direction')
    parser.add_argument('--clearance', type=float, default=-1, help='seal: cm to hold off the body, measured live')
    parser.add_argument('--max-push', type=float, default=-1, help='seal: cm the live correction may add')
    args = parser.parse_args()

    if args.request == 'seal':
        send('seal_tune', {'lift': args.lift, 'clearance': args.clearance, 'max_push': args.max_push}, None)
        print(json.dumps(json.loads((MOD / 'runtime/seal-tune.json').read_text()), indent=1))
        return
    print(json.dumps(send('css_probe', {'request': json.loads(args.request)}, 'css-probe.json'), indent=1))


main()

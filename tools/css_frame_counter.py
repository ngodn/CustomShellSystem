#!/usr/bin/env python3
"""Read engine frame cadence through an already-loaded developer CSS core.

No DLL replacement, assets, input or preferences are changed. The existing
runtime request channel must be authorized. Python 3.14, standard library only.
"""
import argparse
import json
import math
from pathlib import Path
import time
from css import ROOT, GAME, atomic, processes

MOD = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


def cadence(first, last):
    for sample in (first, last):
        if type(sample['frame']) is not int or sample['frame'] < 0:
            raise ValueError('Invalid engine frame counter')
        if not all(math.isfinite(sample[k]) for k in ('sent', 'received')) or sample['received'] < sample['sent']:
            raise ValueError('Invalid request timing')
    count = last['frame'] - first['frame']
    shortest = last['sent'] - first['received']
    longest = last['received'] - first['sent']
    if count <= 0 or shortest <= 0:
        raise ValueError('Counter did not advance or request windows overlap')
    return {'frames': count, 'estimated_engine_hz': count/((shortest+longest)/2),
            'engine_hz_lower_bound': count/longest, 'engine_hz_upper_bound': count/shortest,
            'scope': 'Engine frame-counter cadence with request-latency bounds; not GPU presentation timing.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT/'work'):
        parser.error('Choose a new output file under CustomShellSystem/work')
    if not math.isfinite(args.seconds) or not 3 <= args.seconds <= 30:
        parser.error('Duration must be 3 to 30 seconds')
    pids = processes()
    if len(pids) != 1:
        raise RuntimeError('Expected one running game; this tool does not launch it')
    original = json.loads((MOD/'runtime/loader.json').read_text())
    evidence = {'core': original['core'], 'pids': pids, 'samples': []}

    def probe(request):
        rid = 'frame-count-' + str(time.time_ns())
        start = time.monotonic()
        atomic(MOD/'request.json', {'id': rid, 'action': 'css_probe', 'request': request})
        while time.monotonic()-start < 5:
            if processes() != pids or json.loads((MOD/'runtime/loader.json').read_text())['core'] != original['core']:
                raise RuntimeError('Game or core changed during capture')
            try:
                reply = json.loads((MOD/'runtime/css-probe.json').read_text())
                end = time.monotonic()
                if reply.get('id') == rid:
                    if not reply.get('ok'):
                        raise RuntimeError(reply.get('error', 'Probe failed'))
                    return reply['result'], start, end
            except (FileNotFoundError, json.JSONDecodeError):
                pass
            time.sleep(.02)
        raise TimeoutError('No response from the existing CSS probe')

    try:
        target, _, _ = probe({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})
        if not target:
            raise RuntimeError('KismetSystemLibrary default object is unavailable')
        deadline = time.monotonic()+args.seconds
        while True:
            value, sent, received = probe({'op': 'call', 'target': target, 'function': 'GetFrameCount', 'args': {}})
            evidence['samples'].append({'frame': value['ReturnValue'], 'sent': sent, 'received': received})
            if received >= deadline:
                break
            time.sleep(min(1, max(0, deadline-time.monotonic())))
        evidence['summary'] = cadence(evidence['samples'][0], evidence['samples'][-1])
        print(json.dumps(evidence['summary'], indent=2))
    except Exception as error:
        evidence['error'] = str(error)
        raise
    finally:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(evidence, indent=2, allow_nan=False)+'\n')


if __name__ == '__main__':
    main()

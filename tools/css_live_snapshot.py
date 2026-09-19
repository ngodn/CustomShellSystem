#!/usr/bin/env python3
"""Read the live player's materials, section masks, morphs and spring nodes.

Python 3.14. Development requests only; no appearance mutations. Run while the
player stays in one level. Each request is acknowledged and saved before the
next begins, so failed probes retain their evidence.
"""
import argparse
import json
from pathlib import Path
import time

from css import GAME, ROOT, atomic, wait_json

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


class Probe:
    def __init__(self, log):
        self.log = log

    def send(self, op, **fields):
        request = dict(op=op, **fields)
        rid = str(time.time_ns())
        self.log.write(json.dumps(dict(id=rid, request=request)) + '\n')
        self.log.flush()
        atomic(MOD / 'request.json', dict(id=rid, action='css_probe', request=request))
        reply = wait_json(MOD / 'runtime/css-probe.json', lambda j: j.get('id') == rid)
        self.log.write(json.dumps(reply) + '\n')
        self.log.flush()
        if not reply.get('ok'):
            raise RuntimeError(reply)
        return reply['result']

    def call(self, target, function, **args):
        return self.send('call', target=target, function=function, args=args)['ReturnValue']


def snapshot(probe):
    player = probe.send('player')
    if not player.get('pawn'):
        raise RuntimeError('No playable character')
    mesh = probe.send('get', target=player['pawn'], property='Mesh')
    functions = ('GetPostProcessInstance', 'GetNumMaterials', 'GetMaterial',
                 'IsMaterialSectionShown', 'GetMorphTarget')
    signatures = {fn: probe.send('describe', target=mesh, function=fn) for fn in functions}
    count = probe.call(mesh, 'GetNumMaterials')
    if not 0 < count <= 128:
        raise RuntimeError(f'Unexpected material count: {count}')
    materials = [probe.call(mesh, 'GetMaterial', ElementIndex=i) for i in range(count)]
    shown = [probe.call(mesh, 'IsMaterialSectionShown', MaterialID=i, LODIndex=0)
             for i in range(count)]
    anim = probe.call(mesh, 'GetPostProcessInstance')
    springs = {}
    if anim:
        properties = probe.send('properties', target=anim)
        for prop in properties:
            if prop['name'].startswith('AnimGraphNode_SpringBone'):
                springs[prop['name']] = probe.send('get', target=anim, property=prop['name'])
    # These six names are exported by the V30 candidate, not guessed runtime fields.
    morphs = {name: probe.call(mesh, 'GetMorphTarget', MorphTargetName=name) for name in
              ('FBMBodyTone', 'PBMBreastsSize', 'PBMGlutesSize', 'PBMHipSize',
               'PBMThighsTone', 'PBMWaistWidth')}
    after = probe.send('player')
    if after != player:
        raise RuntimeError('Player changed during snapshot; discard this sample')
    return dict(player=player, mesh=mesh, signatures=signatures, materials=materials,
                hidden_sections=[i for i, visible in enumerate(shown) if not visible],
                post_process=anim, springs=springs, morphs=morphs,
                captured_ns=time.time_ns())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'work'):
        parser.error('Output must be inside CustomShellSystem/work')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.with_suffix('.requests.jsonl').open('x') as log:
        result = snapshot(Probe(log))
    with output.open('x') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(output=str(output), springs=len(result['springs']),
                         materials=len(result['materials']),
                         hidden_sections=result['hidden_sections'], morphs=result['morphs'])))


if __name__ == '__main__':
    main()

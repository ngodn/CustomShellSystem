#!/usr/bin/env python3
"""Inspect V44's live binding without changing the player. Python 3.14.

Exit 2 means no character is loaded, 1 means a failed/incomplete probe, and 0
means the scoped binding checks passed. None of these prove gameplay acceptance.
Optional distance queries read geometry; they do not exercise damage or parry.
"""
import argparse
import json
import math
from pathlib import Path

from css import ROOT, atomic, processes
from css_live_snapshot import Probe

BASE = '/Game/CSSAuthoring/DiagnosticReferences/'
EXPECTED = {
    'mesh': BASE + 'SK_B2PhysicsBound_V1.SK_B2PhysicsBound_V1',
    'skeleton': BASE + 'SKEL_B2GameReferenceMetadata_V2.SKEL_B2GameReferenceMetadata_V2',
    'physics': BASE + 'PA_B2BodyFit_V3.PA_B2BodyFit_V3',
}
ANIM_CLASS = ('/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeHandB2ReferenceV2.'
              'ABP_CSS_ControlRigProbeHandB2ReferenceV2_C')
SIGNATURE_SIZES = {
    'GetPostProcessInstance': {'ReturnValue': 8},
    'GetCollisionEnabled': {'ReturnValue': 1},
    'GetCollisionProfileName': {'ReturnValue': 8},
    'GetSocketLocation': {'InSocketName': 8, 'ReturnValue': 24},
    'K2_GetClosestPointOnPhysicsAsset': {
        'WorldPosition': 24, 'ClosestWorldPosition': 24, 'Normal': 24,
        'BoneName': 8, 'Distance': 4, 'ReturnValue': 1},
    'GetClosestPointOnCollision': {
        'Point': 24, 'OutPointOnBody': 24, 'BoneName': 8, 'ReturnValue': 4},
}


def object_path(value, field='name'):
    return value.get(field, '').partition(' ')[2] if isinstance(value, dict) else None


def inspect(probe, geometry=False):
    result = {'status': 'incomplete', 'gameplay_accepted': False, 'failures': []}
    player = result['player'] = probe.send('player')
    if not player.get('pawn'):
        result['status'] = 'waiting_for_character'
        return result

    def get(target, name):
        props = probe.send('properties', target=target, inherited=True)
        if name not in {p['name'] for p in props}:
            raise RuntimeError(f'Missing reflected property: {name}')
        return probe.send('get', target=target, property=name)

    def call(target, name, **args):
        # Describe resolves the real function before any invocation. Engine
        # signatures are from the pinned UE 5.6.1 headers, not guessed calls.
        signature = probe.send('describe', target=target, function=name)
        result.setdefault('signatures', {})[name] = signature
        if {key: value['size'] for key, value in signature.items()} != SIGNATURE_SIZES[name]:
            raise RuntimeError(f'Reflected signature differs from the pinned engine: {name}')
        return probe.send('call', target=target, function=name, args=args)

    mesh = result['component'] = get(player['pawn'], 'Mesh')
    asset = result['mesh'] = get(mesh, 'SkeletalMesh')
    if object_path(asset) != EXPECTED['mesh']:
        result['failures'].append('The live character is not using the V44 mesh')
    else:
        result['skeleton'] = get(asset, 'Skeleton')
        result['physics'] = get(asset, 'PhysicsAsset')
        result['physics_override'] = get(mesh, 'PhysicsAssetOverride')
        for key in ('skeleton', 'physics'):
            if object_path(result[key]) != EXPECTED[key]:
                result['failures'].append(f'Unexpected {key} binding')
        if result['physics_override'] is not None:
            result['failures'].append('Component has a Physics Asset override')

        anim = result['post_process'] = call(mesh, 'GetPostProcessInstance')['ReturnValue']
        if object_path(anim, 'class') != ANIM_CLASS:
            result['failures'].append('Unexpected or missing V44 post-process instance')
        else:
            props = probe.send('properties', target=anim, inherited=True)
            names = {p['name'] for p in props}
            expected = {'CSSStiffness': 200, 'CSSDamping': 24, 'CSSEnabled': True,
                        'CSSHandEnabled': True, 'CSSHandInputIsV43Compatible': False}
            values = result['rig_inputs'] = {}
            for name in expected.keys() | {'CSSGravity'}:
                if name not in names:
                    result['failures'].append(f'Missing rig input {name}')
                    continue
                values[name] = probe.send('get', target=anim, property=name)
                if name in expected and values[name] != expected[name]:
                    result['failures'].append(f'{name} differs from the trial setting')
            if values.get('CSSGravity') != dict(X=0, Y=0, Z=0):
                result['failures'].append('Hair gravity differs from the accepted zero setting')
        result['can_be_damaged'] = get(player['pawn'], 'bCanBeDamaged')
        result['collision_enabled'] = call(mesh, 'GetCollisionEnabled')['ReturnValue']
        result['collision_profile'] = call(mesh, 'GetCollisionProfileName')['ReturnValue']
        # These fields are observations, not acceptance of game collision rules.
        if geometry and not result['failures']:
            origin = call(mesh, 'GetSocketLocation', InSocketName='pelvis')['ReturnValue']
            if not all(math.isfinite(origin[k]) for k in ('X', 'Y', 'Z')):
                raise RuntimeError('Non-finite pelvis position')
            samples = result['distance_queries'] = []
            for axis in ('X', 'Y', 'Z'):
                point = dict(origin)
                point[axis] += 150
                asset_query = call(mesh, 'K2_GetClosestPointOnPhysicsAsset', WorldPosition=point)
                body_query = call(mesh, 'GetClosestPointOnCollision', Point=point, BoneName='pelvis')
                samples.append(dict(point=point, asset=asset_query, pelvis_body=body_query))
                if not asset_query.get('ReturnValue'):
                    result['failures'].append(f'{axis}: asset distance query failed')
                elif (not isinstance(asset_query.get('Distance'), (int, float))
                      or not math.isfinite(asset_query['Distance']) or asset_query['Distance'] <= 0):
                    result['failures'].append(f'{axis}: asset query returned no finite exterior distance')
                distance = body_query.get('ReturnValue')
                if not isinstance(distance, (int, float)) or not math.isfinite(distance) or distance <= 0:
                    result['failures'].append(f'{axis}: pelvis body did not return an exterior distance')
            result['geometry_scope'] = ('Three exterior samples only. Asset queries ignore collision state; '
                                        'body distances do not prove damage filtering, overlap events or parry.')

    after = result['player_after'] = probe.send('player')
    if after != player or get(player['pawn'], 'Mesh') != mesh or get(mesh, 'SkeletalMesh') != asset:
        result['failures'].append('Player or mesh changed during the sequential reads')
    result['status'] = 'failed' if result['failures'] else 'binding_verified'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--geometry', action='store_true', help='Also read three exterior distance samples')
    args = parser.parse_args()
    out = args.output.resolve()
    log_path = out.with_suffix('.requests.jsonl')
    if not out.is_relative_to(ROOT / 'work') or out.exists() or log_path.exists():
        parser.error('Use a fresh output inside CustomShellSystem/work')
    pids = processes()
    if len(pids) != 1:
        parser.error('Exactly one running game is required')
    out.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open('x') as log:
        try:
            result = inspect(Probe(log), args.geometry)
        except Exception as error:
            result = dict(status='probe_error', error=str(error), gameplay_accepted=False)
    result['processes'] = pids
    atomic(out, result)
    print(json.dumps(dict(output=str(out), **result), indent=2))
    return 2 if result['status'] == 'waiting_for_character' else int(result['status'] != 'binding_verified')


if __name__ == '__main__':
    raise SystemExit(main())

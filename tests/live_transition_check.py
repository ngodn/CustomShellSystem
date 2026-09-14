#!/usr/bin/env python3
"""Reversible cosmetic checks in a safe loaded game with a developer core.

Stage build/cssx-native first, with CSS_TRANSITION_TESTS and CSS_INVENTORY_DEV
ON. This driver never installs a DLL or changes gameplay abilities. Run alone.
"""
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from css import ROOT, atomic, processes, sha, wait_json
from cssx_dev import MOD, BUILD
from cssx_cheat_check import request as host_request


def request(action, **values):
    rid = str(time.time_ns())
    atomic(MOD / 'request.json', dict(action=action, id=rid, **values))
    filename = 'inspection' if action == 'inspect' else 'inventory' if action.startswith('inventory_') else 'status'
    return wait_json(MOD / f'runtime/{filename}.json',
                     lambda j: j.get('last_request' if filename == 'status' else 'id') == rid)


def status():
    return json.loads((MOD / 'runtime/status.json').read_text())


def compact(value):
    return {k: value[k] for k in ('applied', 'mesh', 'shell', 'recovery_pending', 'message')}


def main():
    if not processes():
        raise RuntimeError('Load the game in a safe location first.')
    cache = (BUILD / 'CMakeCache.txt').read_text()
    for option in ('CSS_TRANSITION_TESTS', 'CSS_INVENTORY_DEV'):
        assert f'{option}:BOOL=ON' in cache, f'{option} must be ON'
    loader = json.loads((MOD / 'runtime/loader.json').read_text())
    assert sha(MOD / 'cores' / loader['core']) == sha(BUILD / 'css_core.dll'), 'Stage the current developer core first'
    before = request('inspect')
    assert before['player_ready'] and before['controller_ready']
    assert not any(before[k] for k in ('IsMoveInputIgnored', 'IsLookInputIgnored', 'IsInGameMenu'))
    initial = status()
    assert initial['enabled'] and initial['applied'] and not initial['recovery_pending'], 'An outfit must be applied'
    saved = (MOD / 'state/state.json').read_bytes()
    pawn = host_request({'op': 'player'}, True)['pawn']
    component = host_request({'op': 'get', 'target': pawn, 'property': 'Mesh'}, True)
    animation = host_request({'op': 'get', 'target': component, 'property': 'AnimScriptInstance'}, True)
    evidence = {'before': before, 'initial': compact(initial), 'cycles': []}
    active_effect = False
    menu_open = False

    def recovered():
        result = wait_json(MOD / 'runtime/status.json', lambda j:
                           j.get('applied') == initial['applied'] and j.get('mesh') == initial['mesh']
                           and not j.get('recovery_pending'))
        return compact(result)

    try:
        for _ in range(3):
            cycle = {}
            evidence['cycles'].append(cycle)
            request('test_reset_mesh')
            time.sleep(.4)
            cycle['stock_reset'] = recovered()
            active_effect = True
            request('test_effect', begin=True, parameters=False)
            time.sleep(.4)
            cycle['empty_mid'] = recovered()
            active_effect = False
            active_effect = True
            request('test_effect', begin=True, parameters=True)
            time.sleep(1.2)
            cycle['active_effect'] = compact(status())
            assert cycle['active_effect']['mesh'] != initial['mesh'], 'CSS replaced an active effect'
            assert cycle['active_effect']['recovery_pending'], 'No recovery scheduled'
            request('test_effect', begin=False)
            active_effect = False
            cycle['effect_finished'] = recovered()
        try:
            menu_open = True
            request('inventory_test_key', key='I', down=True)
        finally:
            request('inventory_test_key', key='I', down=False)
        time.sleep(1)
        evidence['inventory_open'] = request('inspect')
        assert evidence['inventory_open']['IsInGameMenu'], 'Native Inventory did not open'
        host_request({'op': 'menu.close'}, True)
        menu_open = False
        time.sleep(.5)
        evidence['after'] = request('inspect')
        assert not evidence['after']['IsInGameMenu']
        assert not evidence['after']['IsMoveInputIgnored'] and not evidence['after']['IsLookInputIgnored']
        after_animation = host_request({'op': 'get', 'target': component, 'property': 'AnimScriptInstance'}, True)
        assert animation == after_animation, 'Gameplay animation instance changed'
        assert (MOD / 'state/state.json').read_bytes() == saved, 'Saved preferences changed'
        print('Three stock/empty-MID/active-effect cycles and native Inventory cleanup passed')
    finally:
        try:
            if active_effect and status()['mesh'] != initial['mesh']:
                request('test_effect', begin=False)
            if menu_open:
                host_request({'op': 'menu.close'}, True)
        finally:
            evidence['final'] = compact(status())
            output = ROOT / 'work/cssx-native/material-recovery-live.json'
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(json.dumps(evidence, indent=2) + '\n')
            print(output)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Local F8 input check. Requires a focused game, no shortcuts and cheats off."""
import json
import time
from pathlib import Path
from css import atomic, wait_json
from cssx_dev import MOD
from cssx_cheat_check import request, event, model
from cssx_power_check import get


def key(name, down):
    rid = str(time.time_ns())
    atomic(MOD / 'request.json', {'id': rid, 'action': 'inventory_test_key', 'key': name, 'down': down})
    result = wait_json(MOD / 'runtime/inventory.json', lambda j: j.get('id') == rid)
    if result.get('error'):
        raise RuntimeError(result['error'])


def press(name):
    key(name, True)
    try:
        time.sleep(.12)
    finally:
        key(name, False)
    time.sleep(.3)


def main():
    settings = MOD / 'state/extensions/eins0fx.cheat-menu.json'
    before = json.loads(settings.read_text())
    assert not before.get('bindings'), 'Preserve existing user shortcuts; do not run this check'
    assert request({'op': 'input.focus'}, True) is True
    assert request({'op': 'menu.status'}, True)['menu_open'] is False
    assert not model()['disable_all']['enabled']
    pawn = request({'op': 'player'}, True)['pawn']
    assert get(pawn, 'bCanBeDamaged') is True
    evidence = {}
    try:
        event('binding_action', value='god')
        event('binding_key', value='F8')
        press('F8')
        assert get(pawn, 'bCanBeDamaged') is True, 'Draft shortcut fired'
        event('apply_settings')
        time.sleep(.3)
        key('F8', True)
        time.sleep(.5)
        assert get(pawn, 'bCanBeDamaged') is False, 'Shortcut did not enable God'
        time.sleep(.5)
        assert get(pawn, 'bCanBeDamaged') is False, 'Held shortcut repeated'
        key('F8', False)
        time.sleep(.2)
        press('F8')
        assert get(pawn, 'bCanBeDamaged') is True, 'Second press did not restore damage'
        evidence['gameplay'] = 'draft ignored; held press fires once; second press restores original'
        press('I')
        assert request({'op': 'menu.status'}, True)['menu_open'] is True
        press('F8')
        assert get(pawn, 'bCanBeDamaged') is True, 'Shortcut fired in Inventory'
        request({'op': 'menu.close'}, True)
        time.sleep(.6)
        assert request({'op': 'menu.status'}, True)['menu_open'] is False
        evidence['inventory'] = 'shortcut suppressed; native Inventory closes through host service'
        evidence['saved'] = json.loads(settings.read_text())['bindings']
        assert evidence['saved'] == {'god': 'F8'}
    finally:
        key('F8', False)
        key('I', False)
        if model()['disable_all']['enabled']:
            event('disable_all')
        if model()['discard_changes']['enabled']:
            event('discard_changes')
        event('clear_bindings')
        if model()['apply_settings']['enabled']:
            event('apply_settings')
        request({'op': 'menu.close'}, True)
        evidence['damage_restored'] = get(pawn, 'bCanBeDamaged')
        evidence['bindings_restored'] = json.loads(settings.read_text())['bindings']
        output = Path(__file__).resolve().parents[1] / 'work/cssx-native/bindings-live-check.json'
        output.write_text(json.dumps(evidence, indent=2) + '\n')
    assert evidence['damage_restored'] is True and evidence['bindings_restored'] == {}
    print('Native F8 shortcut, held-key suppression, Inventory gate, saved binding and cleanup passed')


if __name__ == '__main__':
    main()

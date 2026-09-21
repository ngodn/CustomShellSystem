#!/usr/bin/env python3
"""Live, reversible Cheat Menu check through the CSSX dev channel.

Checks that a draft edit changes no gameplay, Discard reverts it, Apply enables
God (bCanBeDamaged false on the pawn), and Turn off all cheats restores the
original flag. Run in a safe spot. Leaves no cheat enabled even on failure.
"""
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cssx import GAME, request  # noqa: E402

ID = 'eins0fx.cheat-menu'


def rt(value):
    r = request(GAME, value)
    if not r['ok']:
        raise RuntimeError(r.get('error', str(r)))
    return r['result']


def engine(value):
    return rt({'op': 'engine', 'request': value})


def event(cid, **fields):
    return rt({'op': 'event', 'extension': ID, 'event': {'id': cid, **fields}})


def model():
    m = rt({'op': 'model', 'extension': ID})
    return {c['id']: c for s in m['sections'] for c in s['controls']}


def main():
    c = model()
    assert not c['apply_settings'].get('enabled', True), 'Apply or discard existing edits before this test'
    assert not c['god']['value'], 'Disable God before this test'
    pawn = engine({'op': 'player'})['pawn']
    before = engine({'op': 'get', 'target': pawn, 'property': 'bCanBeDamaged'})
    assert before is True, 'Test requires a damageable pawn'
    results = []
    try:
        event('god', value=True); time.sleep(.5)
        results.append(('draft leaves gameplay unchanged', engine({'op': 'get', 'target': pawn, 'property': 'bCanBeDamaged'}) == before))
        results.append(('apply notices the edit', model()['apply_settings']['enabled']))
        event('discard_changes'); results.append(('discard reverts the draft', model()['god']['value'] is False))
        event('god', value=True); event('apply_settings'); time.sleep(.6)
        results.append(('apply enables God', engine({'op': 'get', 'target': pawn, 'property': 'bCanBeDamaged'}) is False))
        event('disable_all'); time.sleep(.6)
        results.append(('disable all restores the original flag', engine({'op': 'get', 'target': pawn, 'property': 'bCanBeDamaged'}) == before))
        results.append(('no pending edits after cleanup', not model()['apply_settings']['enabled']))
    finally:
        c = model()
        if c['disable_all'].get('enabled', False):
            event('disable_all')
        if c['discard_changes'].get('enabled', False):
            event('discard_changes')
    for name, ok in results:
        print(('PASS ' if ok else 'FAIL ') + name)
    final = engine({'op': 'get', 'target': pawn, 'property': 'bCanBeDamaged'})
    print('final bCanBeDamaged', final)
    if not all(ok for _, ok in results) or final != before:
        sys.exit(1)


if __name__ == '__main__':
    main()

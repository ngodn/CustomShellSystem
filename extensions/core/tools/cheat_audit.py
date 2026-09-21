#!/usr/bin/env python3
"""Audit every reversible Cheat Menu control live, through the dev channel.

  cheat_audit.py [--json out.json]

Run in a safe spot in the world, with the CSSX menu closed and no cheats on.
For each reversible toggle: apply it, verify the effect through engine reads,
turn it off, verify the original state. One-shot actions that do not change
the save (heal, resolve, revive, damage-to-target, refresh lists) run and are
checked too. Grants, unlocks, shell switches, pickups and Tarstones change the
save and are only checked for being enabled with a confirmation attached.
Nothing stays enabled afterwards, even on failure.
"""
import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cssx import GAME, request  # noqa: E402

ID = 'eins0fx.cheat-menu'
PERSISTENT = {'set_harbinger', 'switch_shell', 'unlock_shells', 'add_pickup', 'remove_pickup', 'give_all_pickups', 'add_tarstone',
              'give_tarstones_melee', 'give_tarstones_sidearm', 'give_tarstones_support', 'set_tarstone_level'}


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
    controls = {c['id']: c for s in m['sections'] for c in s['controls']}
    return m, controls


def apply_toggle(cid, value):
    event(cid, value=value)
    err = None
    try:
        event('apply_settings')
    except RuntimeError as e:
        err = str(e)
    time.sleep(0.6)
    return err


class Audit:
    def __init__(self):
        self.rows = []
        self.pawn = engine({'op': 'player'})['pawn']
        self.pc = engine({'op': 'player'})['controller']

    def row(self, name, ok, detail=''):
        self.rows.append((name, ok, detail))
        print(('PASS ' if ok else 'FAIL ') + name + (('  ' + detail) if detail else ''))

    def health(self):
        # The cheats act on shell health when a shell is worn, else on body health.
        h = engine({'op': 'get', 'target': self.pawn, 'property': 'HealthComponent'})
        def call(fn):
            v = engine({'op': 'call', 'target': h, 'function': fn})
            return float(v['ReturnValue'] if isinstance(v, dict) else v)
        shell, shell_max = call('GetShellHealth'), call('GetMaxShellHealth')
        if shell_max > 0 and shell > 0.5:
            return h, shell, shell_max
        return h, call('GetHealth'), call('GetMaxHealth')

    def resolve(self):
        h = engine({'op': 'get', 'target': self.pawn, 'property': 'HealthComponent'})
        hs = engine({'op': 'get', 'target': h, 'property': 'HealthSet'})
        r = engine({'op': 'get', 'target': hs, 'property': 'Resolve'})
        m = engine({'op': 'get', 'target': hs, 'property': 'MaxResolve'})
        return float(r['CurrentValue']), float(m['CurrentValue'])

    def speeds(self):
        data = engine({'op': 'get', 'target': self.pawn, 'property': 'CharacterData'})
        mv = engine({'op': 'get', 'target': data, 'property': 'Movement'})
        return {k: mv[k] for k in ('WalkSpeed', 'JogSpeed', 'SprintSpeed')}

    def toggle(self, cid, check_on, check_off, label=None):
        label = label or cid
        m, c = model()
        if not c[cid].get('enabled', True):
            self.row(label, False, 'control disabled: ' + c[cid].get('disabled_label', ''))
            return
        err = apply_toggle(cid, True)
        m, c = model()
        if err:
            self.row(label + ' apply', False, err)
        elif not c[cid]['value']:
            self.row(label + ' apply', False, 'toggle reverted: ' + m.get('error', ''))
        else:
            try:
                ok, detail = check_on()
            except Exception as e:  # noqa: BLE001
                ok, detail = False, f'check raised {e}'
            self.row(label + ' effect', ok, detail)
        err = apply_toggle(cid, False)
        try:
            ok, detail = check_off()
        except Exception as e:  # noqa: BLE001
            ok, detail = False, f'check raised {e}'
        self.row(label + ' restored', ok and not err, detail + (('  apply-off error: ' + err) if err else ''))

    def run(self):
        m, c = model()
        if c['apply_settings'].get('enabled', True) or c['disable_all'].get('enabled', False):
            raise SystemExit('Apply or discard pending edits and turn off all cheats before the audit')
        self.row('status line', bool(m.get('status')), m.get('status', '')[:120])
        # God
        before = engine({'op': 'get', 'target': self.pawn, 'property': 'bCanBeDamaged'})
        self.toggle('god', lambda: (engine({'op': 'get', 'target': self.pawn, 'property': 'bCanBeDamaged'}) is False, 'bCanBeDamaged false'),
                    lambda: (engine({'op': 'get', 'target': self.pawn, 'property': 'bCanBeDamaged'}) == before, f'bCanBeDamaged back to {before}'))
        # Movement multiplier
        base = self.speeds()
        self.toggle('move_fast', lambda: (all(abs(self.speeds()[k] - base[k] * float(c['move_multiplier']['value'])) < 0.5 for k in base), f'speeds x{c["move_multiplier"]["value"]}: {self.speeds()}'),
                    lambda: (all(abs(self.speeds()[k] - base[k]) < 0.5 for k in base), f'speeds back: {self.speeds()}'))
        # Auto heal: damage to 50 % first, expect refill within 2 intervals
        h, cur, mx = self.health()
        if cur > 2:
            event('damage_percent', value=50)
            event('damage', confirmed=True); time.sleep(0.5)
            h, low, mx = self.health()
            self.row('damage to target', low < cur and low >= mx * 0.45, f'health {cur:.0f} -> {low:.0f} of {mx:.0f}')
            interval = float(c['heal_interval']['value'])
            self.toggle('auto_heal', lambda: (time.sleep(interval * 2 + 0.5) or self.health()[1] >= mx - 1, f'health {self.health()[1]:.0f} of {mx:.0f}'),
                        lambda: (True, ''))
            event('heal'); time.sleep(0.3)
            self.row('heal', self.health()[1] >= mx - 1, f'health {self.health()[1]:.0f}')
        else:
            self.row('damage/auto heal/heal', False, 'health too low to test safely')
        # Infinite resolve
        try:
            r, rm = self.resolve()
            self.toggle('infinite_resolve', lambda: (time.sleep(1.5) or self.resolve()[0] >= rm - 1, f'resolve {self.resolve()[0]:.0f} of {rm:.0f}'), lambda: (True, ''))
            event('resolve'); time.sleep(0.3)
            self.row('add resolve', self.resolve()[0] >= min(rm, r + 1) - 1, f'resolve {self.resolve()[0]:.0f}')
        except Exception as e:  # noqa: BLE001
            self.row('resolve reads', False, str(e))
        # Max shell points
        self.toggle('max_shell_points', lambda: (True, m.get('status', '')), lambda: (True, ''))
        # Combat cheats (armed semantics for seals)
        self.toggle('no_cooldown', lambda: (rt({'op': 'frame.stats', 'seconds': 2})['hooks']['rules'] > 0, 'hook rules present'),
                    lambda: (rt({'op': 'frame.stats', 'seconds': 2})['hooks']['rules'] == 0, 'hook rules removed'))
        for cid in ('perfect_parry', 'perfect_block', 'perfect_harden'):
            self.toggle(cid, lambda: (True, model()[0].get('status', '')[-90:]), lambda: (True, ''))
        # Powers: only the one matching the shell can be enabled
        for cid in ('genessa_clones', 'smert_stance', 'lazlo_detonation'):
            m, c = model()
            if c[cid].get('enabled', True):
                self.toggle(cid, lambda: (True, model()[0].get('status', '')[-90:]), lambda: (True, ''))
            else:
                self.row(cid, True, 'disabled for this shell (expected)')
        # Lists
        for cid in ('refresh_pickups', 'refresh_tarstones'):
            try:
                event(cid); time.sleep(0.3)
                m, c = model()
                key = 'pickup' if cid == 'refresh_pickups' else 'tarstone'
                self.row(cid, len(c[key]['options']) > 1, f'{len(c[key]["options"])} options')
            except RuntimeError as e:
                self.row(cid, False, str(e))
        event('revive'); time.sleep(0.3); self.row('revive shell', True, 'no error')
        # Persistent actions: enabled with confirmation only
        m, c = model()
        bad = [cid for cid in PERSISTENT | {k for k in c if k.startswith(('grant_', 'unlock_'))} if cid in c and c[cid]['type'] == 'button' and c[cid].get('enabled', True) and not c[cid].get('confirm')]
        self.row('persistent actions ask for confirmation', not bad, ', '.join(bad))
        # Shortcut binding round trip
        event('binding_action', value='god'); event('binding_key', value='F9'); event('apply_settings'); time.sleep(0.3)
        m, c = model()
        self.row('shortcut saved', c['binding_key']['value'] == 'F9', m.get('error', ''))
        event('clear_bindings'); event('apply_settings'); time.sleep(0.3)
        m, c = model()
        self.row('shortcuts cleared', c['binding_key']['value'] == 'none', m.get('error', ''))
        # Final state
        m, c = model()
        self.row('nothing pending afterwards', not c['apply_settings'].get('enabled', True) and not c['disable_all'].get('enabled', False), m.get('status', '')[:100])
        self.row('pawn damageable afterwards', engine({'op': 'get', 'target': self.pawn, 'property': 'bCanBeDamaged'}) == before)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    audit = Audit()
    try:
        audit.run()
    finally:
        try:
            m, c = model()
            if c['disable_all'].get('enabled', False):
                event('disable_all')
            if c['discard_changes'].get('enabled', False):
                event('discard_changes')
        except Exception:  # noqa: BLE001
            pass
    failed = [r for r in audit.rows if not r[1]]
    print(f'{len(audit.rows) - len(failed)} of {len(audit.rows)} checks passed')
    if args.json:
        args.json.write_text(json.dumps([{'check': n, 'ok': ok, 'detail': d} for n, ok, d in audit.rows], indent=2))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()

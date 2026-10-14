"""Interrupt one facial performance with Axatana's secondary attack and restore the view."""
import argparse
import json
import math
from pathlib import Path
import shutil
import subprocess
import time

from capture_eve_surface_variant import ROOT, describe, processes, sha, atomic
from check_peony_live_presets import engine, customize, request, GAME
from check_sidearm_aim_visibility import MOD, geometry_restored


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--clip', default='css.eve.dance.irisout')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists():
        raise RuntimeError('Use a new evidence directory')
    original, pids = describe(), processes()
    core = json.loads((MOD / 'core.json').read_text())
    if len(pids) != 1 or sha(MOD / 'cores' / core['file']) != args.core_sha256:
        raise RuntimeError('Expected running build not found')
    if core['file'] not in Path(f'/proc/{pids[0]}/maps').read_text():
        raise RuntimeError('Expected core is not mapped')
    before = json.loads((MOD / 'state/state.json').read_text())
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
    perf = lambda **fields: customize({'action': 'performance', **fields})
    if original['busy'] or original['expression']['performance'] or perf(op='status')['active']:
        raise RuntimeError('Another operation owns the appearance')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    report = dict(passed=False, core=core, core_sha256=args.core_sha256,
                  clip=args.clip, original=original, samples=[], movement_input=False,
                  camera_input=False, attack_input=False)
    log = (out / 'requests.jsonl').open('x')

    def send(value):
        log.write(json.dumps({'request': value}) + '\n'); log.flush()
        result = engine(value)
        log.write(json.dumps({'result': result}) + '\n'); log.flush()
        return result

    signatures = set()

    def call(target, function, **fields):
        identity = (target['$object'], function)
        if identity not in signatures:
            send({'op': 'describe', 'target': target, 'function': function})
            signatures.add(identity)
        return send({'op': 'call', 'target': target, 'function': function,
                     'args': fields}).get('ReturnValue')

    def get(target, name):
        return send({'op': 'get', 'target': target, 'property': name})

    player = send({'op': 'player'})
    body = get(player['pawn'], 'Mesh')
    health = get(player['pawn'], 'HealthComponent')
    health_set = get(health, 'HealthSet')
    health_before = get(health_set, 'Health')
    input_component = call(player['pawn'], 'GetSpartaInputComponent')
    weapons = get(player['pawn'], 'WeaponsComponent')
    asc = get(player['pawn'], 'AbilitySystemComponent')
    equipment = get(weapons, 'EquippedWeapons')
    primary = next(v['value'] for v in equipment['$map'] if v['key']['TagName'] == 'Weapon.Slot.Primary.Right')
    if not primary['class'].endswith('WP_Axatana_Axe_C'):
        raise RuntimeError('This stationary fixture was prepared for the equipped Axatana axe')
    player_input = get(player['controller'], 'PlayerInput')
    mappings = get(player_input, 'EnhancedActionMappings')
    if not any(r['Action']['name'].endswith('.IA_Attack_Secondary') and
               r['Key']['KeyName'] == 'RightMouseButton' for r in mappings):
        raise RuntimeError('Secondary attack is not mapped to the expected button')

    def active_attacks():
        return [v['Ability'] for v in get(asc, 'ActivatableAbilities')['Items']
                if v['ActiveCount'] and '/Axatana/' in v['Ability']['class'] and
                '/GA_Player_Attack_' in v['Ability']['class']]

    def geometry():
        return {k: call(player[t], f) for k, t, f in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('view_target', 'controller', 'GetViewTarget'), ('control_rotation', 'controller', 'GetControlRotation'))}

    def keys():
        return send({'op': 'input.keys', 'target': player['controller'],
                     'keys': ['W', 'A', 'S', 'D', 'F', 'SpaceBar', 'LeftControl', 'LeftMouseButton', 'RightMouseButton']})

    baseline = geometry()
    report.update(player=player, geometry_before=baseline, health_before=health_before)
    window = lambda name: json.loads(subprocess.check_output(['hyprctl', name, '-j']))
    prior_window = window('activewindow')
    game_window = next(w for w in window('clients') if w['pid'] == pids[0] and w['class'] == 'steam_app_2584270')

    def focus(address):
        if not address.startswith('0x') or any(c not in '0123456789abcdef' for c in address[2:]):
            raise RuntimeError('Invalid window address')
        subprocess.run(['hyprctl', 'dispatch', 'hl.dsp.focus({window="address:' + address + '"})'],
                       check=True, capture_output=True)

    def guard(require_stationary=False):
        if processes() != pids or send({'op': 'player'}) != player:
            raise RuntimeError('Player or process changed')
        current = describe()
        if any(current[k] != original[k] for k in ('shell', 'outfit', 'variant', 'customize', 'idle_override')):
            raise RuntimeError('Appearance changed')
        if json.loads((MOD / 'state/state.json').read_text()) != before or get(weapons, 'EquippedWeapons') != equipment:
            raise RuntimeError('Settings or equipment changed')
        if get(health_set, 'Health') != health_before or call(health, 'IsDeadOrDying'):
            raise RuntimeError('Health changed')
        g = geometry()
        if any(g[k] != baseline[k] for k in ('control_rotation', 'view_target')):
            raise RuntimeError('Camera changed')
        distance = math.dist(list(g['position'].values()), list(baseline['position'].values()))
        if distance > 200:
            raise RuntimeError('Attack travelled beyond the prepared restoration range')
        if any(keys().values()):
            raise RuntimeError('Unexpected held input')
        if require_stationary:
            anim = call(body, 'GetAnimInstance')
            if get(anim, 'Speed') > .05 or call(anim, 'IsAnyMontagePlaying'):
                raise RuntimeError('Gameplay animation is still active')
        menu = send({'op': 'menu.status'})
        if menu['menu_open'] or menu['game_menu_open']:
            raise RuntimeError('A menu is open')
        return g

    token, mouse = None, False
    atomic(out / 'result.json', report)
    try:
        focus(game_window['address']); guard()
        if int(subprocess.check_output(['xdotool', 'getactivewindow', 'getwindowpid'], stderr=subprocess.PIPE)) != pids[0]:
            raise RuntimeError('X11 focus is not the game')
        pointer = window('cursorpos')
        if not all(game_window['at'][i] <= pointer[k] < game_window['at'][i] + game_window['size'][i]
                   for i, k in enumerate(('x', 'y'))):
            raise RuntimeError('Pointer is outside the game')
        if get(input_component, 'ActiveBlocks'):
            raise RuntimeError('Gameplay input is blocked')
        started = report['started'] = perf(op='start', id=args.clip, seconds=.5, rate=1.)
        token = started['token']
        deadline = time.monotonic() + 12
        while True:
            playing = perf(op='status')
            if playing['active'] and playing['weight'] == 1 and describe()['expression']['performance']:
                break
            if time.monotonic() > deadline:
                raise TimeoutError('Facial performance did not start')
            time.sleep(.1)
        report['playing'] = playing
        guard()
        if window('activewindow').get('address') != game_window['address']:
            raise RuntimeError('Game lost focus')
        report['attack_input'] = True; atomic(out / 'result.json', report)
        mouse = True
        subprocess.run(['xdotool', 'mousedown', '3'], check=True, capture_output=True, timeout=3)
        time.sleep(.1)
        report['attack_key_observed'] = keys()['RightMouseButton']
        report['attacks_while_pressed'] = active_attacks()
        subprocess.run(['xdotool', 'mouseup', '3'], check=True, capture_output=True, timeout=3)
        mouse = False
        deadline = time.monotonic() + 8
        while True:
            anim = call(body, 'GetAnimInstance')
            row = dict(active_attacks=active_attacks(), performance=perf(op='status'), expression=describe()['expression'],
                       ability=call(player['controller'], 'GetLastActivatedAbility'),
                       montage=call(anim, 'GetCurrentActiveMontage'), geometry=geometry())
            report['samples'].append(row); atomic(out / 'result.json', report)
            if not row['performance']['active'] and not row['expression']['performance']:
                break
            if time.monotonic() > deadline:
                raise TimeoutError('Attack did not interrupt performance')
            time.sleep(.1)
        stopped = report['stopped'] = row['performance']
        if stopped['token'] != token or stopped['seconds'] >= stopped['duration'] or not stopped['reason']:
            raise RuntimeError('No early interruption of the requested performance')
        if not report['attack_key_observed']:
            raise RuntimeError('Attack input not observed')
        action_objects = report['attacks_while_pressed'] + [a for v in report['samples'] for a in v['active_attacks']]
        if not action_objects:
            raise RuntimeError('No native Axatana action was observed')
        time.sleep(3)
        report['after_action'] = guard(require_stationary=True)
        report['action_passed'] = True
    except Exception as error:
        report['error'] = repr(error)
    finally:
        try:
            if mouse:
                subprocess.run(['xdotool', 'mouseup', '3'], check=True, capture_output=True, timeout=3)
            current = perf(op='status')
            if token is not None and current['active'] and current['token'] == token:
                report['forced_stop'] = perf(op='stop', token=token)
            time.sleep(.5)
            end_geometry = guard(require_stationary=True)
            if end_geometry != baseline:
                restored = call(player['pawn'], 'K2_SetActorLocationAndRotation',
                                NewLocation=baseline['position'], NewRotation=baseline['rotation'],
                                bSweep=False, bTeleport=True)
                if not restored:
                    raise RuntimeError('Position restoration failed')
            report['geometry_after'] = geometry()
            report['geometry_restored'] = geometry_restored(baseline, report['geometry_after'])
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(out / 'state-after.json', after)
            report['state_restored'] = before == after
            report['cine_restored'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine
            report['health_unchanged'] = get(health_set, 'Health') == health_before
            report['keys_released'] = not any(keys().values())
        except Exception as error:
            report['restore_error'] = repr(error)
        if window('activewindow').get('address') == game_window['address'] and any(
                w['address'] == prior_window['address'] for w in window('clients')):
            focus(prior_window['address'])
        report['focus_restored'] = window('activewindow').get('address') == prior_window['address']
        report['passed'] = bool(report.get('action_passed') and not report.get('forced_stop') and
                               not report.get('error') and not report.get('restore_error') and all(
            report.get(k) for k in ('geometry_restored', 'state_restored', 'cine_restored',
                                   'health_unchanged', 'keys_released', 'focus_restored')))
        atomic(out / 'result.json', report)
        log.close()
    print(json.dumps({k: v for k, v in report.items() if isinstance(v, (str, bool))}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

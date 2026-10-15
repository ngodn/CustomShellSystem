"""Interrupt facial playback with a mapped native action, then restore state."""
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
from css_live_snapshot import Probe
import cssx


def fast_wait(path, predicate, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            data = json.loads(path.read_text())
            if predicate(data):
                return data
        except (OSError, json.JSONDecodeError):
            pass
        time.sleep(.005)
    raise TimeoutError(f'No matching acknowledgement in {path}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--clip', default='css.eve.dance.irisout')
    parser.add_argument('--action', choices=('secondary_attack', 'secondary_combo', 'dodge', 'shell_ability', 'parry'), default='secondary_attack')
    parser.add_argument('--ground-proof', type=Path, help='Required checked corridor for the stationary Stray dodge or combo')
    parser.add_argument('--start-evidence', type=Path, help='Verified previously occupied staircase point for the combo')
    args = parser.parse_args()
    if args.action in ('secondary_combo', 'shell_ability', 'parry'):
        cssx.wait_json = fast_wait
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
                  camera_input=False, action=args.action, action_input=False)
    log = (out / 'requests.jsonl').open('x')

    def send(value):
        log.write(json.dumps({'request': value, 'monotonic': time.monotonic()}) + '\n'); log.flush()
        result = engine(value)
        log.write(json.dumps({'result': result, 'monotonic': time.monotonic()}) + '\n'); log.flush()
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
    dodge = args.action == 'dodge'
    combo = args.action == 'secondary_combo'
    summon = args.action == 'shell_ability'
    parry = args.action == 'parry'
    input_action, input_key = (('IA_Parry_Press', 'Q') if parry else
                              ('IA_Shell_Ability', 'F') if summon else
                              ('IA_Dodge', 'SpaceBar') if dodge else ('IA_Attack_Secondary', 'RightMouseButton'))
    if not any(r['Action']['name'].endswith('.' + input_action) and
               r['Key']['KeyName'] == input_key for r in mappings):
        raise RuntimeError('Action is not mapped to the expected key')
    down, up = ((['keydown', 'q'], ['keyup', 'q']) if parry else
                (['keydown', 'f'], ['keyup', 'f']) if summon else
                (['keydown', 'space'], ['keyup', 'space']) if dodge else (['mousedown', '3'], ['mouseup', '3']))

    def action_matches(ability):
        name = ability['class']
        if parry:
            return '/Parry/GA_Parry_Action_Single.' in name
        if summon:
            return '/CorruptedGenessa/Abilities/GA_AstralClones_Action.' in name
        return ('/Dash/GA_Dash_CorruptedGenessa.' in name if dodge else
                '/Axatana/' in name and '/GA_Player_Attack_' in name)

    def active_actions():
        return [v['Ability'] for v in get(asc, 'ActivatableAbilities')['Items']
                if v['ActiveCount'] and action_matches(v['Ability'])]

    def geometry():
        return {k: call(player[t], f) for k, t, f in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('view_target', 'controller', 'GetViewTarget'), ('control_rotation', 'controller', 'GetControlRotation'))}

    def keys():
        return send({'op': 'input.keys', 'target': player['controller'],
                     'keys': ['W', 'A', 'S', 'D', 'F', 'Q', 'SpaceBar', 'LeftControl', 'LeftMouseButton', 'RightMouseButton']})

    if parry:
        report['resolve_before'] = get(health_set, 'Resolve')
        if active_actions():
            raise RuntimeError('Another parry is already active')

    baseline = geometry()
    expected_control = baseline['control_rotation']
    test_yaw = None
    if dodge or combo or summon:
        if args.ground_proof is None or player['shell'] != 'CharacterId.Player.Darkform.CorruptedGenessa':
            raise RuntimeError('This action requires the checked Stray corridor')
        proof = json.loads(args.ground_proof.read_text())
        if (proof['player'] != player or proof['position'] != baseline['position'] or
                proof['control_rotation'] != baseline['control_rotation'] or
                proof['misses'] or proof['penetrating'] or len(proof['samples']) != 93):
            raise RuntimeError('Checked corridor does not match this player and view')
        if not all(v['trace']['ReturnValue'] and not v['trace']['OutHit']['bStartPenetrating']
                   for v in proof['samples']):
            raise RuntimeError('Checked corridor has an unsupported sample')
        if {(v['distance'], v['width']) for v in proof['samples']} != {
                (distance, width) for distance in range(0, 751, 25) for width in (-50, 0, 50)}:
            raise RuntimeError('Checked corridor coverage is incomplete')
        test_yaw = proof['test_yaw']
        if not math.isfinite(test_yaw) or abs(test_yaw - baseline['rotation']['Yaw']) > .001:
            raise RuntimeError('Corridor is not the checked uphill heading')
    if dodge:
        specs = [v for v in get(asc, 'ActivatableAbilities')['Items'] if action_matches(v['Ability'])]
        if len(specs) != 1 or len(specs[0]['NonReplicatedInstances']) != 1:
            raise RuntimeError('Expected one native Stray dash instance')
        dash = specs[0]['NonReplicatedInstances'][0]
        report['dash_parameters'] = {name: get(dash, name) for name in
                                     ('DashDistance', 'StrafeDashDistance', 'DashTravelTime')}
        if report['dash_parameters'] != {'DashDistance': 425., 'StrafeDashDistance': 465.,
                                        'DashTravelTime': .3499999940395355}:
            raise RuntimeError('Native dash parameters differ from the prepared corridor')
    if summon:
        specs = [v for v in get(asc, 'ActivatableAbilities')['Items'] if action_matches(v['Ability'])]
        if len(specs) != 1 or len(specs[0]['NonReplicatedInstances']) != 1 or specs[0]['ActiveCount']:
            raise RuntimeError('Need one inactive native Stray summon instance')
        ability = specs[0]['NonReplicatedInstances'][0]
        report['secondary_eligible'] = call(ability, 'HasSecondaryClone')
        if not report['secondary_eligible']:
            raise RuntimeError('This fixture expects the upgraded two-clone save')
        report['resolve_before'] = get(health_set, 'Resolve')
        report['summon_samples'] = []
        observer = Probe(log)

        def observe_summon():
            observed = observer.send('astral.observe', poses=False)
            status = json.loads((MOD / 'runtime/status.json').read_text())['astral']
            if observed['status'] != 'observed':
                raise RuntimeError('Astral observer is unavailable')
            return dict(observed=observed, status=status, monotonic=time.monotonic())

        report['summon_before'] = observe_summon()
        initial = report['summon_before']
        if any(a['active'] for a in initial['observed']['actors']) or initial['status']['active'] or initial['status']['cached']:
            raise RuntimeError('Existing doubles are still active or cached')
        if before['doubles']['stray'] not in ('default', 'css'):
            raise RuntimeError('Unexpected Stray doubles mode')
    if dodge or combo or summon:
        report['ground_proof'] = str(args.ground_proof.resolve())
        report['ground_sha256'] = sha(args.ground_proof)
    if combo:
        counter_class = send({'op': 'find', 'path': '/Game/Sparta/Core/Player/Components/BPC_Player_ComboCounter.BPC_Player_ComboCounter_C'})
        counter = call(player['pawn'], 'GetComponentByClass', ComponentClass=counter_class)
        report['combo_counter'] = counter
        report['combo_history_before'] = get(counter, 'DebugComboHistory')
        if get(counter, 'CurrentComboCount') != 0:
            raise RuntimeError('Another combo is already active')
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
        if g['control_rotation'] != expected_control or g['view_target'] != baseline['view_target']:
            raise RuntimeError('Camera changed')
        distance = math.dist(list(g['position'].values()), list(baseline['position'].values()))
        if distance > (800 if dodge or combo or summon else 200):
            raise RuntimeError('Action travelled beyond the prepared restoration range')
        if any(keys().values()):
            raise RuntimeError('Unexpected held input')
        if (dodge or summon) and any(call(player['controller'], 'GetMoveInput').values()):
            raise RuntimeError('Stationary action requires zero movement input')
        if require_stationary:
            anim = call(body, 'GetAnimInstance')
            if get(anim, 'Speed') > .05 or call(anim, 'IsAnyMontagePlaying'):
                raise RuntimeError('Gameplay animation is still active')
        menu = send({'op': 'menu.status'})
        if menu['menu_open'] or menu['game_menu_open']:
            raise RuntimeError('A menu is open')
        return g

    token, mouse, camera_changed = None, False, False
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
        if dodge or summon:
            camera_changed = True
            call(player['controller'], 'SetControlRotation', NewRotation={**expected_control, 'Yaw': test_yaw})
            expected_control = call(player['controller'], 'GetControlRotation')
            report['test_control_rotation'] = expected_control
            if abs(expected_control['Yaw'] - test_yaw) > .001:
                raise RuntimeError('Prepared heading did not apply')
        if combo:
            if args.start_evidence is None:
                raise RuntimeError('Combo requires verified start evidence')
            prior = json.loads(args.start_evidence.read_text())
            if (not prior['passed'] or prior['player'] != player or prior['geometry_before'] != baseline or
                    not prior['health_unchanged'] or prior['core_sha256'] != args.core_sha256):
                raise RuntimeError('Previously occupied staircase point is not valid for this fixture')
            report['test_start'] = prior['after_action']['position']
            report['start_evidence'] = str(args.start_evidence.resolve())
            report['start_evidence_sha256'] = sha(args.start_evidence)
            if math.dist(list(report['test_start'].values()), list(baseline['position'].values())) > 750:
                raise RuntimeError('Previously occupied start is outside the checked corridor')
            if not call(player['pawn'], 'K2_SetActorLocationAndRotation',
                        NewLocation=report['test_start'], NewRotation=baseline['rotation'], bSweep=False, bTeleport=True):
                raise RuntimeError('Prepared staircase start did not apply')
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
        report['action_input'] = True; atomic(out / 'result.json', report)
        mouse = True
        pressed_at = time.monotonic()
        subprocess.run(['xdotool', *down], check=True, capture_output=True, timeout=3)
        time.sleep(.02)
        report['action_key_observed'] = keys()[input_key]
        subprocess.run(['xdotool', *up], check=True, capture_output=True, timeout=3)
        mouse = False
        report['key_hold_seconds'] = time.monotonic() - pressed_at
        report['actions_after_release'] = active_actions()
        if combo:
            report['combo_inputs'] = [{'pressed_at': pressed_at, 'count': get(counter, 'CurrentComboCount'),
                                       'active_actions': report['actions_after_release']}]
            report['timeline_only'] = False
            report['timeline'] = []
            deadline = time.monotonic() + 6
            saw_block, second_pressed = False, False
            while time.monotonic() < deadline:
                row = dict(start=time.monotonic(), blocks=get(input_component, 'ActiveBlocks'),
                           queue=get(input_component, 'InputQueue'),
                           count=get(counter, 'CurrentComboCount'),
                           actions=active_actions(), position=call(player['pawn'], 'K2_GetActorLocation'))
                row['end'] = time.monotonic()
                report['timeline'].append(row)
                atomic(out / 'result.json', report)
                if math.dist(list(row['position'].values()), list(baseline['position'].values())) > 750:
                    raise RuntimeError('Combo reached the observation distance limit')
                saw_block = saw_block or bool(row['blocks'])
                if saw_block and not row['blocks'] and not second_pressed:
                    if row['count'] != 1 or get(counter, 'CurrentComboCount') != 1:
                        raise RuntimeError('First combo stage expired before the next press')
                    if window('activewindow').get('address') != game_window['address'] or any(keys().values()):
                        raise RuntimeError('Input or focus changed before second attack')
                    if get(health_set, 'Health') != health_before:
                        raise RuntimeError('Health changed before second attack')
                    second_pressed = True
                    second = dict(pressed_at=time.monotonic(), before=row)
                    report['second_input'] = second
                    mouse = True
                    subprocess.run(['xdotool', *down], check=True, capture_output=True, timeout=3)
                    time.sleep(.08)
                    second['key_observed'] = keys()['RightMouseButton']
                    subprocess.run(['xdotool', *up], check=True, capture_output=True, timeout=3)
                    mouse = False
                    second['released_at'] = time.monotonic()
                    if not second['key_observed']:
                        raise RuntimeError('Second attack input was not observed')
            if not second_pressed:
                raise RuntimeError('No first-attack block transition observed')
            report['combo_history_after'] = get(counter, 'DebugComboHistory')
            history = report['combo_history_after']
            prefix = report['combo_history_before']
            if history[:len(prefix)] != prefix:
                raise RuntimeError('Combo history changed outside the expected append')
            report['combo_events'] = history[len(prefix):]
            for index in (1, 2):
                if not any(f'GA_Player_Attack_Axatana_Axe_B{index}_' in event for event in report['combo_events']):
                    raise RuntimeError(f'No native combo stage B{index} recorded')
        deadline = time.monotonic() + 8
        while True:
            anim = call(body, 'GetAnimInstance')
            row = dict(active_actions=active_actions(), performance=perf(op='status'), expression=describe()['expression'],
                       ability=call(player['controller'], 'GetLastActivatedAbility'),
                       montage=call(anim, 'GetCurrentActiveMontage'), geometry=geometry())
            report['samples'].append(row); atomic(out / 'result.json', report)
            if not row['performance']['active'] and not row['expression']['performance']:
                break
            if time.monotonic() > deadline:
                raise TimeoutError('Action did not interrupt performance')
            time.sleep(.1)
        stopped = report['stopped'] = row['performance']
        if stopped['token'] != token or stopped['seconds'] >= stopped['duration'] or not stopped['reason']:
            raise RuntimeError('No early interruption of the requested performance')
        if not report['action_key_observed']:
            raise RuntimeError('Action input not observed')
        action_objects = report['actions_after_release'] + [a for v in report['samples'] for a in v['active_actions']]
        if not action_objects:
            raise RuntimeError('No expected native action was observed')
        if summon:
            deadline = time.monotonic() + 12
            while True:
                observed = observe_summon()
                report['summon_samples'].append(observed)
                atomic(out / 'result.json', report)
                if any(a['active'] for a in observed['observed']['actors']):
                    break
                if time.monotonic() > deadline:
                    raise RuntimeError('No summoned clone observed after interruption')
                guard()
                time.sleep(.2)
        time.sleep(3)
        report['after_action'] = guard(require_stationary=True)
        if combo:
            report['combo_count_after'] = get(counter, 'CurrentComboCount')
            if report['combo_count_after'] != 0:
                raise RuntimeError('Combo count did not reset naturally')
        report['action_passed'] = True
    except Exception as error:
        report['error'] = repr(error)
    finally:
        try:
            if mouse:
                subprocess.run(['xdotool', *up], check=True, capture_output=True, timeout=3)
            current = perf(op='status')
            if token is not None and current['active'] and current['token'] == token:
                report['forced_stop'] = perf(op='stop', token=token)
            if summon and report['action_input']:
                deadline, inactive_since = time.monotonic() + 65, None
                while True:
                    guard()
                    observed = observe_summon()
                    report['summon_samples'].append(observed)
                    atomic(out / 'result.json', report)
                    active = any(a['active'] for a in observed['observed']['actors'])
                    status = observed['status']
                    if not active and status['active'] == status['cached'] == 0 and not status['error']:
                        inactive_since = inactive_since or time.monotonic()
                        if time.monotonic() - inactive_since >= 3:
                            break
                    else:
                        inactive_since = None
                    if time.monotonic() > deadline:
                        raise RuntimeError('Summoned clones did not expire and clean up')
                    time.sleep(.3)
                report['summon_after'] = observed
                report['resolve_after'] = get(health_set, 'Resolve')
                report['resolve_restored'] = report['resolve_after'] == report['resolve_before']
                report['actions_after_cleanup'] = active_actions()
                report['input_blocks_after_cleanup'] = get(input_component, 'ActiveBlocks')
                if report['actions_after_cleanup'] or report['input_blocks_after_cleanup']:
                    raise RuntimeError('Summon still owns the ability or blocks gameplay input')
            time.sleep(.5)
            if combo:
                deadline = time.monotonic() + 8
                while call(call(body, 'GetAnimInstance'), 'IsAnyMontagePlaying'):
                    if time.monotonic() > deadline:
                        raise RuntimeError('Native combo did not finish before restoration')
                    time.sleep(.1)
            end_geometry = guard(require_stationary=True)
            if end_geometry != baseline:
                restored = call(player['pawn'], 'K2_SetActorLocationAndRotation',
                                NewLocation=baseline['position'], NewRotation=baseline['rotation'],
                                bSweep=False, bTeleport=True)
                if not restored:
                    raise RuntimeError('Position restoration failed')
            if camera_changed:
                call(player['controller'], 'SetControlRotation', NewRotation=baseline['control_rotation'])
            report['geometry_after'] = geometry()
            report['geometry_restored'] = geometry_restored(baseline, report['geometry_after'])
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(out / 'state-after.json', after)
            report['state_restored'] = before == after
            report['cine_restored'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine
            report['health_unchanged'] = get(health_set, 'Health') == health_before
            report['keys_released'] = not any(keys().values())
            if parry:
                report['resolve_after'] = get(health_set, 'Resolve')
                report['resolve_restored'] = report['resolve_after'] == report['resolve_before']
                report['actions_after_cleanup'] = active_actions()
                report['input_blocks_after_cleanup'] = get(input_component, 'ActiveBlocks')
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
        if summon:
            report['passed'] = report['passed'] and bool(report.get('summon_after') and report.get('resolve_restored'))
        if parry:
            report['passed'] = report['passed'] and bool(report.get('resolve_restored')) and not (
                report.get('actions_after_cleanup') or report.get('input_blocks_after_cleanup'))
        atomic(out / 'result.json', report)
        log.close()
    print(json.dumps({k: v for k, v in report.items() if isinstance(v, (str, bool))}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

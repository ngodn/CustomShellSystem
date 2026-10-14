"""Simulate the established Ctrl aim binding, inspect Naylshotte and restore the view."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, describe, processes, sha
from check_peony_live_presets import customize, engine, request
sys.path.insert(0, str(ROOT / 'tools'))
from css import atomic
import css_capture

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


def geometry_restored(before, after):
    return (all(before[key] == after[key] for key in ('position', 'view_target', 'control_rotation')) and
            all(abs((after['rotation'][axis] - before['rotation'][axis] + 180) % 360 - 180) <= 1e-8
                for axis in ('Pitch', 'Yaw', 'Roll')))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--fire-once', action='store_true',
                        help='Send one focused left click while aiming and record its normal Resolve cost')
    parser.add_argument('--interrupt-performance', choices=('css.eve.dance.irisout', 'css.eve.dance.comecloser'),
                        help='Start this facial clip and require actual aiming to interrupt it')
    parser.add_argument('--input-mode', choices=('console', 'keyboard'), default='keyboard',
                        help='Keyboard requires the focused game; console is an unverified diagnostic alternative')
    args = parser.parse_args()
    if args.fire_once and args.input_mode != 'keyboard':
        parser.error('--fire-once requires the focused keyboard input route')
    out = args.output.resolve()
    pids = processes()
    if out.exists() or len(pids) != 1:
        raise RuntimeError('Need one running game and a new output directory')

    def focused_game():
        window = json.loads(subprocess.check_output(['hyprctl', 'activewindow', '-j']))
        return window.get('pid') == pids[0] and window.get('class') == 'steam_app_2584270'

    if args.input_mode == 'keyboard' and not focused_game():
        raise RuntimeError('Bring the game to the foreground before the keyboard check; no input was sent')
    core = json.loads((MOD / 'core.json').read_text())
    if sha(MOD / 'cores' / core['file']) != args.core_sha256 or core['file'] not in Path(f'/proc/{pids[0]}/maps').read_text():
        raise RuntimeError('Expected core is not installed and mapped')
    before = json.loads((MOD / 'state/state.json').read_text())
    original = describe()
    performance = lambda **fields: customize({'action': 'performance', **fields})
    if before['misc_rules'].get('sidearm') != {'mode': 'in_use'} or original['busy'] or original['expression']['performance']:
        raise RuntimeError('Need settled appearance and Only when in use sidearm visibility')
    if performance(op='status')['active']:
        raise RuntimeError('An existing performance owns the player')
    player = engine({'op': 'player'})

    def get(target, prop):
        return engine({'op': 'get', 'target': target, 'property': prop})

    def call(target, function, **fields):
        return engine({'op': 'call', 'target': target, 'function': function, 'args': fields}).get('ReturnValue')

    wc = get(player['pawn'], 'WeaponsComponent')
    equipped = get(wc, 'EquippedWeapons')
    weapon = next(e['value'] for e in equipped['$map'] if e['key']['TagName'] == 'Weapon.Slot.Sidearm')
    if not weapon['class'].endswith('.WP_NailShotgun_C'):
        raise RuntimeError('Need the equipped Naylshotte')
    mesh = get(weapon, 'WeaponMesh')
    effects = get(weapon, 'BPC_WeaponEffects')
    effect = get(effects, 'SpawnedPermanent')
    body = get(player['pawn'], 'Mesh')
    health = get(player['pawn'], 'HealthComponent')
    health_set = get(health, 'HealthSet')
    input_component = call(player['pawn'], 'GetSpartaInputComponent')
    if not effect:
        raise RuntimeError('Need the permanent effect for this check')
    signatures = {}
    system = engine({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})
    for target, function in ((mesh, 'GetAttachSocketName'), (effect, 'WasRecentlyRendered'),
                             (body, 'GetAnimInstance'), (health, 'IsDeadOrDying'),
                             (system, 'ExecuteConsoleCommand'),
                             (player['controller'], 'GetLastActivatedAbility'),
                             (player['pawn'], 'K2_SetActorLocationAndRotation')):
        signatures[function] = engine({'op': 'describe', 'target': target, 'function': function})
    if set(signatures['WasRecentlyRendered']) != {'Tolerance', 'ReturnValue'}:
        raise RuntimeError('Unexpected rendering observation signature')
    transform_sig = signatures['K2_SetActorLocationAndRotation']
    if not {'NewLocation', 'NewRotation', 'bSweep', 'bTeleport', 'ReturnValue'} <= set(transform_sig):
        raise RuntimeError('Unexpected transform restoration signature')

    def geometry():
        return {key: call(target, function) for key, target, function in (
            ('position', player['pawn'], 'K2_GetActorLocation'),
            ('rotation', player['pawn'], 'K2_GetActorRotation'),
            ('view_target', player['controller'], 'GetViewTarget'),
            ('control_rotation', player['controller'], 'GetControlRotation'))}

    baseline = geometry()
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok']:
        raise RuntimeError('Cannot inspect CINE state')
    buttons = {c['id']: c.get('enabled', True) for s in cine['result']['sections'] for c in s['controls']}
    if not buttons['take'] and buttons['exit']:
        raise RuntimeError('CINE must be stopped or between takes')

    def resources():
        return dict(health=get(health_set, 'Health'), resolve=get(health_set, 'Resolve'))

    initial_resources = resources()
    if args.fire_once and initial_resources['resolve']['CurrentValue'] < 40:
        raise RuntimeError('Insufficient Resolve for the single-shot fixture')

    def focused_x11_game():
        if not focused_game():
            return False
        window = json.loads(subprocess.check_output(['hyprctl', 'activewindow', '-j']))
        pointer = json.loads(subprocess.check_output(['hyprctl', 'cursorpos', '-j']))
        if not (window['at'][0] <= pointer['x'] < window['at'][0] + window['size'][0] and
                window['at'][1] <= pointer['y'] < window['at'][1] + window['size'][1]):
            return False
        pid = subprocess.check_output(['xdotool', 'getactivewindow', 'getwindowpid'],
                                      stderr=subprocess.PIPE, timeout=3)
        return int(pid.strip()) == pids[0]

    if args.fire_once and not focused_x11_game():
        raise RuntimeError('X11 focus does not match the current game')

    def infusion():
        return dict(tags=get(weapon, 'WeaponEffectTags'), effect=get(effects, 'SpawnedPermanent'),
                    asset=get(effect, 'Asset'), active=get(effect, 'bIsActive'))

    initial_infusion = infusion()

    def keys():
        return engine({'op': 'input.keys', 'target': player['controller'],
                       'keys': ['LeftControl', 'W', 'A', 'S', 'D', 'SpaceBar', 'LeftMouseButton', 'RightMouseButton']})

    def guard(aiming=False):
        if processes() != pids or engine({'op': 'player'}) != player:
            raise RuntimeError('Game or player changed')
        current = describe()
        if any(current[k] != original[k] for k in ('shell', 'outfit', 'variant', 'customize', 'idle_override')) or current['busy']:
            raise RuntimeError('Appearance changed')
        if json.loads((MOD / 'state/state.json').read_text()) != before or get(wc, 'EquippedWeapons') != equipped:
            raise RuntimeError('Settings or equipment changed')
        if infusion() != initial_infusion or call(health, 'IsDeadOrDying'):
            raise RuntimeError('Infusion or player life state changed')
        current_geometry = geometry()
        if any(current_geometry[k] != baseline[k] for k in ('view_target', 'control_rotation')):
            raise RuntimeError('Camera input detected')
        distance = sum((current_geometry['position'][k] - baseline['position'][k]) ** 2 for k in ('X', 'Y', 'Z')) ** .5
        if distance > 10:
            raise RuntimeError('Player moved beyond the stationary aim tolerance')
        down = keys()
        if any(value for key, value in down.items() if key != 'LeftControl') or (not aiming and down['LeftControl']):
            raise RuntimeError('Concurrent input detected')
        menu = engine({'op': 'menu.status'})
        if menu['menu_open'] or menu['game_menu_open'] or get(input_component, 'ActiveBlocks'):
            raise RuntimeError('Gameplay input is blocked')

    def snapshot():
        anim = call(body, 'GetAnimInstance')
        return dict(socket=call(mesh, 'GetAttachSocketName'), actor_hidden=get(weapon, 'bHidden'),
                    mesh_hidden=get(mesh, 'bHiddenInGame'), equip_state=get(weapon, 'CurrentEquipState'),
                    effect_rendered=call(effect, 'WasRecentlyRendered', Tolerance=.2),
                    speed=get(anim, 'Speed'), anim=anim, expression=describe()['expression'])

    guard()
    initial = snapshot()
    if 'stowed' not in initial['socket'].lower() or not initial['actor_hidden'] or not initial['mesh_hidden'] or initial['speed'] > .05:
        raise RuntimeError('Need a stationary stowed hidden sidearm')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    atomic(out / 'signatures.json', signatures)
    css_capture.MEDIA = out
    report = dict(passed=False, core=core, process_before=pids, core_sha256=args.core_sha256,
                  initial=initial, geometry_before=baseline, infusion_before=initial_infusion,
                  input_mode=args.input_mode, input='Ctrl aim down/up', firing_input=False,
                  resources_before=initial_resources, fire_requested=args.fire_once,
                  interrupt_performance=args.interrupt_performance,
                  movement_input=False, camera_input=False)
    atomic(out / 'result.json', report)

    def aim_input(down):
        if processes() != pids or engine({'op': 'player'}) != player:
            raise RuntimeError('Player changed before input')
        if args.input_mode == 'keyboard':
            if down and not focused_game():
                raise RuntimeError('Game lost focus before aim input')
            return css_capture.command('inventory_test_key', key='Ctrl', down=down)
        command = 'Input.' + ('+' if down else '-') + 'key LeftControl'
        with (out / 'input-requests.jsonl').open('a') as log:
            log.write(json.dumps({'command': command, 'time_ns': time.time_ns()}) + '\n')
        call(system, 'ExecuteConsoleCommand', WorldContextObject=player['pawn'],
             Command=command, SpecificPlayer=player['controller'])
        return {'command': command}

    pressed = False
    performance_token = None
    try:
        guard()
        if args.interrupt_performance:
            report['performance_started'] = performance(op='start', id=args.interrupt_performance, seconds=.5, rate=1.)
            performance_token = report['performance_started']['token']
            deadline = time.monotonic() + 8
            while True:
                clip = performance(op='status')
                expression = describe()['expression']
                if not clip['active'] or clip['token'] != performance_token:
                    raise RuntimeError('Requested performance lost ownership before aim')
                if clip['weight'] == 1 and expression['performance']:
                    break
                if time.monotonic() >= deadline:
                    raise RuntimeError('Facial performance did not become active')
                time.sleep(.1)
            report['performance_before_aim'] = clip
            report['expression_before_aim'] = expression
            atomic(out / 'result.json', report)
        pressed = True
        report['press_reply'] = aim_input(True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            aimed = snapshot()
            if 'stowed' not in aimed['socket'].lower() and not aimed['actor_hidden'] and not aimed['mesh_hidden']:
                break
            time.sleep(.1)
        report['aimed'] = aimed
        report['aim_key_observed'] = keys()['LeftControl']
        guard(aiming=True)
        report['aim_screenshot'] = str(css_capture.shot('aim'))
        if not report['aim_key_observed'] or 'stowed' in aimed['socket'].lower() or aimed['actor_hidden'] or aimed['mesh_hidden']:
            raise RuntimeError('Aim did not draw and reveal the sidearm')
        report['aim_check_passed'] = True
        if args.interrupt_performance:
            deadline = time.monotonic() + 5
            while True:
                clip = performance(op='status')
                expression = describe()['expression']
                if clip['token'] != performance_token:
                    raise RuntimeError('Performance ownership changed during interruption')
                if not clip['active'] and not expression['performance']:
                    break
                if time.monotonic() >= deadline:
                    raise RuntimeError('Aiming left the facial performance active')
                time.sleep(.1)
            report['performance_after_aim'] = clip
            report['expression_after_aim'] = expression
            if not clip['reason'] or clip['seconds'] >= clip['duration']:
                raise RuntimeError('No early gameplay interruption was recorded')
            report['interruption_check_passed'] = True
        if args.fire_once:
            guard(aiming=True)
            if resources() != initial_resources or not focused_x11_game():
                raise RuntimeError('Resources or focus changed before the shot')
            report['ability_before_shot'] = call(player['controller'], 'GetLastActivatedAbility')
            report['firing_input'] = True
            atomic(out / 'result.json', report)
            try:
                subprocess.run(['xdotool', 'mousedown', '1'], capture_output=True, check=True, timeout=3)
                time.sleep(.12)
                report['fire_key_observed'] = keys()['LeftMouseButton']
            finally:
                subprocess.run(['xdotool', 'mouseup', '1'], capture_output=True, check=True, timeout=3)
            report['focus_after_click'] = focused_game()
            if not report['fire_key_observed']:
                raise RuntimeError('Game did not observe the mouse button; firing is unverified')
            deadline = time.monotonic() + 3
            report['shot_observations'] = []
            while True:
                observation = dict(resources=resources(), ability=call(player['controller'], 'GetLastActivatedAbility'),
                                   sidearm=snapshot())
                report['shot_observations'].append(observation)
                if observation['resources']['resolve']['CurrentValue'] < initial_resources['resolve']['CurrentValue']:
                    break
                if time.monotonic() > deadline:
                    raise RuntimeError('No Resolve expenditure observed after the single click')
                time.sleep(.1)
            time.sleep(.6)
            report['resources_after_shot'] = resources()
            report['resolve_drop_observed'] = initial_resources['resolve']['CurrentValue'] - min(
                row['resources']['resolve']['CurrentValue'] for row in report['shot_observations'])
            if report['resolve_drop_observed'] <= 0 or report['resources_after_shot']['health'] != initial_resources['health']:
                raise RuntimeError('Unexpected resources after firing')
            guard(aiming=True)
            report['fire_screenshot'] = str(css_capture.shot('after-shot'))
            report['fire_check_passed'] = True
    except Exception as error:
        report['error'] = repr(error)
    finally:
        try:
            if pressed:
                report['release_reply'] = aim_input(False)
            if performance_token is not None:
                clip = performance(op='status')
                if clip['token'] == performance_token and clip['active']:
                    report['cleanup_stop'] = performance(op='stop', token=performance_token)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                stowed = snapshot()
                if 'stowed' in stowed['socket'].lower() and stowed['actor_hidden'] and stowed['mesh_hidden'] and not stowed['effect_rendered']:
                    break
                time.sleep(.2)
            report['stowed'] = stowed
            guard()
            report['stowed_screenshot'] = str(css_capture.shot('stowed'))
            current = geometry()
            report['geometry_after_aim'] = current
            if current != baseline:
                time.sleep(.5)
                if geometry() != current or stowed['speed'] > .05:
                    raise RuntimeError('Player has not stopped; transform restoration skipped')
                restored = call(player['pawn'], 'K2_SetActorLocationAndRotation', NewLocation=baseline['position'],
                                NewRotation=baseline['rotation'], bSweep=False, bTeleport=True)
                if not restored:
                    raise RuntimeError('Transform restoration failed')
                time.sleep(.5)
            report['geometry_after'] = geometry()
            report['geometry_restored'] = geometry_restored(baseline, report['geometry_after'])
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(out / 'state-after.json', after)
            report['state_restored'] = after == before
            report['resources_after'] = resources()
            report['infusion_preserved'] = infusion() == initial_infusion
            report['cine_restored'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine['result']
            report['keys_released'] = not any(keys().values())
            report['process_after'] = processes()
            report['passed'] = bool(report.get('aim_check_passed') and (not args.fire_once or report.get('fire_check_passed')) and
                (not args.interrupt_performance or report.get('interruption_check_passed')) and 'stowed' in stowed['socket'].lower() and
                stowed['actor_hidden'] and stowed['mesh_hidden'] and not stowed['effect_rendered'] and
                report['geometry_restored'] and report['state_restored'] and report['infusion_preserved'] and
                report['cine_restored'] and report['keys_released'] and processes() == pids)
        except Exception as error:
            report['restore_error'] = repr(error)
        atomic(out / 'result.json', report)
    print(json.dumps({k: v for k, v in report.items() if k not in ('press_reply', 'release_reply', 'initial', 'infusion_before', 'geometry_before', 'geometry_after', 'geometry_after_aim')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

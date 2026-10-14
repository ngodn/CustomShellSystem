"""Simulate the established Ctrl aim binding, inspect Naylshotte and restore the view."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, describe, processes, sha
from check_peony_live_presets import engine, request
sys.path.insert(0, str(ROOT / 'tools'))
from css import atomic
import css_capture

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--input-mode', choices=('console', 'keyboard'), default='keyboard',
                        help='Keyboard requires the focused game; console is an unverified diagnostic alternative')
    args = parser.parse_args()
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
    if before['misc_rules'].get('sidearm') != {'mode': 'in_use'} or original['busy'] or original['expression']['performance']:
        raise RuntimeError('Need settled appearance and Only when in use sidearm visibility')
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
    input_component = call(player['pawn'], 'GetSpartaInputComponent')
    if not effect:
        raise RuntimeError('Need the permanent effect for this check')
    signatures = {}
    system = engine({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})
    for target, function in ((mesh, 'GetAttachSocketName'), (effect, 'WasRecentlyRendered'),
                             (body, 'GetAnimInstance'), (health, 'IsDeadOrDying'),
                             (system, 'ExecuteConsoleCommand'),
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
    if not cine['ok'] or not next(c for s in cine['result']['sections'] for c in s['controls'] if c['id'] == 'take').get('enabled', True):
        raise RuntimeError('CINE must be between takes')

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
    try:
        guard()
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
    except Exception as error:
        report['error'] = repr(error)
    finally:
        try:
            if pressed:
                report['release_reply'] = aim_input(False)
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
            report['state_restored'] = json.loads((MOD / 'state/state.json').read_text()) == before
            report['infusion_preserved'] = infusion() == initial_infusion
            report['cine_restored'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine['result']
            report['keys_released'] = not any(keys().values())
            report['process_after'] = processes()
            report['passed'] = bool(report.get('aim_check_passed') and 'stowed' in stowed['socket'].lower() and
                stowed['actor_hidden'] and stowed['mesh_hidden'] and not stowed['effect_rendered'] and
                report['geometry_after'] == baseline and report['state_restored'] and report['infusion_preserved'] and
                report['cine_restored'] and report['keys_released'] and processes() == pids)
        except Exception as error:
            report['restore_error'] = repr(error)
        atomic(out / 'result.json', report)
    print(json.dumps({k: v for k, v in report.items() if k not in ('press_reply', 'release_reply', 'initial', 'infusion_before', 'geometry_before', 'geometry_after', 'geometry_after_aim')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

"""Check a saved profile in mirrored Stray form, then restore Faithful and settings."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, describe, processes, sha
from check_peony_live_presets import engine, request
sys.path.insert(0, str(ROOT / 'tools'))
from css import atomic, wait_json

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
LIVING = 'CharacterId.Player.Shell.Genessa'
STRAY = 'CharacterId.Player.Darkform.CorruptedGenessa'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    pids = processes()
    if out.exists() or len(pids) != 1:
        raise RuntimeError('Need one running game and a new output directory')
    core = json.loads((MOD / 'core.json').read_text())
    if sha(MOD / 'cores' / core['file']) != args.core_sha256 or not any(
            line.rstrip().endswith('/' + core['file']) for line in Path(f'/proc/{pids[0]}/maps').read_text().splitlines()):
        raise RuntimeError('Expected core is not installed and mapped')
    original = describe()
    before = json.loads((MOD / 'state/state.json').read_text())
    if original['shell'] != LIVING or original['busy'] or original['expression']['performance']:
        raise RuntimeError('Need Faithful with no pending appearance or facial performance')
    living = before['selections'][LIVING]
    separate = before['selections'].get(STRAY)
    if not separate or (separate['outfit'], separate['variant']) == (living['outfit'], living['variant']):
        raise RuntimeError('Need distinct existing Faithful and Stray selections for the negative control')
    if len(before['presets']) >= 64:
        raise RuntimeError('No free temporary profile slot')
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok'] or not next(c for s in cine['result']['sections'] for c in s['controls'] if c['id'] == 'take').get('enabled', True):
        raise RuntimeError('CINE is not between takes')
    player = engine({'op': 'player'})

    def get(target, prop):
        return engine({'op': 'get', 'target': target, 'property': prop})

    def call(target, function):
        engine({'op': 'describe', 'target': target, 'function': function})
        return engine({'op': 'call', 'target': target, 'function': function, 'args': {}})['ReturnValue']

    def stable_guard():
        current = engine({'op': 'player'})
        if processes() != pids or any(current[k] != player[k] for k in ('pawn', 'controller')):
            raise RuntimeError('Game or player changed')
        menu = engine({'op': 'menu.status'})
        health = get(current['pawn'], 'HealthComponent')
        mesh = get(current['pawn'], 'Mesh')
        anim = call(mesh, 'GetAnimInstance')
        if menu['menu_open'] or menu['game_menu_open'] or call(health, 'IsDeadOrDying'):
            raise RuntimeError('Player is not in a living gameplay state')
        if get(anim, 'Speed') > .05 or call(anim, 'IsAnyMontagePlaying'):
            raise RuntimeError('Movement or montage is active')

    def geometry():
        return {key: call(target, function) for key, target, function in (
            ('position', player['pawn'], 'K2_GetActorLocation'),
            ('rotation', player['pawn'], 'K2_GetActorRotation'),
            ('view_target', player['controller'], 'GetViewTarget'),
            ('control_rotation', player['controller'], 'GetControlRotation'))}

    stable_guard()
    original_geometry = geometry()
    original_asset = call(get(player['pawn'], 'Mesh'), 'GetSkeletalMeshAsset')
    name = 'css-diagnostic-form-' + str(time.time_ns())
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    report = dict(passed=False, process_before=pids, core=core, core_sha256=args.core_sha256,
                  temporary_profile=name, geometry_before=original_geometry, original_asset=original_asset,
                  form_requests=True, movement_input=False, camera_input=False, samples=[])
    expected = copy.deepcopy(before)
    expected['presets'][name] = {k: copy.deepcopy(before[k]) for k in
        ('selections', 'walk_animation', 'animation_choices', 'misc_rules', 'doubles')}
    saved = False
    changed_form = False

    with (out / 'requests.jsonl').open('x') as log:
        def command(action, **fields):
            value = dict(id=str(time.time_ns()), action=action, **fields)
            log.write(json.dumps({'request': value}) + '\n'); log.flush()
            atomic(MOD / 'request.json', value)
            status = wait_json(MOD / 'runtime/status.json', lambda s: s.get('last_request') == value['id'])
            log.write(json.dumps({'status': status}) + '\n'); log.flush()
            return status

        def wait_form(shell):
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                current = engine({'op': 'player'})
                if current['shell'] == shell:
                    active = describe()
                    if not active['busy'] and (active['outfit'], active['variant']) == (living['outfit'], living['variant']):
                        try:
                            stable_guard()
                            return active
                        except RuntimeError as error:
                            if str(error) != 'Movement or montage is active':
                                raise
                time.sleep(.25)
            raise TimeoutError('Form or mirrored appearance did not settle: ' + shell)

        try:
            if json.loads((MOD / 'state/state.json').read_text()) != before:
                raise RuntimeError('Saved settings changed before the test')
            command('save_profile', name=name)
            saved = True
            wait_json(MOD / 'state/state.json', lambda s: s == expected)
            command('harbinger_mirror', value=True)
            expected['harbinger_mirror'] = True
            wait_json(MOD / 'state/state.json', lambda s: s == expected)
            stable_guard()
            changed_form = True
            command('genessa_form', mode='stray')
            wait_form(STRAY)
            for label in ('first-load', 'repeated-load'):
                stable_guard()
                command('load_profile', name=name)
                active = wait_form(STRAY)
                wait_json(MOD / 'state/state.json', lambda s: s == expected)
                asset = call(get(player['pawn'], 'Mesh'), 'GetSkeletalMeshAsset')
                if asset != original_asset or active['customize'] != original['customize']:
                    raise RuntimeError('Mirrored profile did not preserve the actual mesh and customization')
                report['samples'].append(dict(label=label, shell=active['shell'], outfit=active['outfit'],
                    variant=active['variant'], mesh=asset, independent_stray_choice_preserved=True))
                atomic(out / (label + '.json'), active)
            report['checks_passed'] = True
        except Exception as error:
            report['error'] = repr(error)
        finally:
            try:
                stable_guard()
                current = json.loads((MOD / 'state/state.json').read_text())
                if current != expected:
                    raise RuntimeError('Concurrent or unexpected saved-state change; automatic restoration stopped')
                if changed_form:
                    command('genessa_form', mode='faithful')
                    wait_form(LIVING)
                command('harbinger_mirror', value=before['harbinger_mirror'])
                expected['harbinger_mirror'] = before['harbinger_mirror']
                wait_json(MOD / 'state/state.json', lambda s: s == expected)
                if saved:
                    command('delete_profile', name=name)
                    expected['presets'].pop(name)
                after = wait_json(MOD / 'state/state.json', lambda s: s == before)
                atomic(out / 'state-after.json', after)
                final = wait_form(LIVING)
                report['saved_state_equal'] = after == before
                report['appearance_equal'] = final['customize'] == original['customize'] and call(
                    get(player['pawn'], 'Mesh'), 'GetSkeletalMeshAsset') == original_asset
                report['geometry_after_forms'] = geometry()
                if report['geometry_after_forms'] != original_geometry:
                    current_geometry = report['geometry_after_forms']
                    delta = sum((current_geometry['position'][axis] - original_geometry['position'][axis]) ** 2
                                for axis in ('X', 'Y', 'Z')) ** .5
                    if delta > 500 or any(current_geometry[k] != original_geometry[k] for k in ('view_target', 'control_rotation')):
                        raise RuntimeError('Unexpected player/view displacement; position restoration stopped')
                    stable_guard()
                    time.sleep(.5)
                    if geometry() != current_geometry:
                        raise RuntimeError('Player moved after form completion; position restoration stopped')
                    engine({'op': 'describe', 'target': player['pawn'], 'function': 'K2_SetActorLocationAndRotation'})
                    args = dict(NewLocation=original_geometry['position'], NewRotation=original_geometry['rotation'],
                                bSweep=False, bTeleport=True)
                    log.write(json.dumps({'restore_player_transform': args}) + '\n'); log.flush()
                    reply = engine({'op': 'call', 'target': player['pawn'], 'function': 'K2_SetActorLocationAndRotation', 'args': args})
                    if not reply['ReturnValue']:
                        raise RuntimeError('Player transform restoration failed')
                    report['position_restored_after_form_displacement_cm'] = delta
                    time.sleep(.5)
                report['geometry_after'] = geometry()
                report['geometry_equal'] = report['geometry_after'] == original_geometry
                report['cine_model_equal'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine['result']
                report['process_after'] = processes()
                report['passed'] = bool(report.get('checks_passed') and report['saved_state_equal'] and
                    report['appearance_equal'] and report['geometry_equal'] and report['cine_model_equal'] and processes() == pids)
            except Exception as error:
                report['restore_error'] = repr(error)
            atomic(out / 'result.json', report)
    print(json.dumps({k: v for k, v in report.items() if k not in ('samples', 'geometry_before', 'geometry_after', 'original_asset')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

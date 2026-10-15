"""Bracket the old core with matched visibility-repair captures on the current core."""
import copy
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time
import traceback

from capture_eve_surface_variant import ROOT, describe, identity, processes, sha, atomic
from check_peony_live_presets import engine, request, GAME
from check_sidearm_aim_visibility import MOD, geometry_restored
import css_capture
from css import wait_json
from css_frame_profile import summarize

OUT = ROOT / 'work/three-outfit-rig-physics/visibility-cost-20261015'
CORES = {
    'candidate': ('css_core-development-5f04a31900c8e323-nested-visibility.dll',
                  '5f04a31900c8e323a27f6ed5650649595015b6dea6600f6682db582dc8fcd619'),
    'baseline': ('css_core-development-4a4402a210f4d1e8-state-recovery.dll',
                 '4a4402a210f4d1e8c706726348237908be80472ef7df129b886ee657e48a5857'),
}


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUT)
    args = parser.parse_args()
    OUT = args.output.resolve()
    assert OUT.is_relative_to((ROOT / 'work/three-outfit-rig-physics').resolve())
    assert not OUT.exists()
    pids = processes()
    assert len(pids) == 1
    original = describe()
    assert identity(original)[1:] == ('eins0fx.seduxtress', 'peony')
    assert not original['busy'] and not original['expression']['performance'] and not original['idle_override']
    state = json.loads((MOD / 'state/state.json').read_text())
    expected_state = copy.deepcopy(state)
    assert state['misc_rules']['sidearm'] == {'mode': 'in_use'}
    selector = json.loads((MOD / 'core.json').read_text())
    assert selector == {'file': CORES['candidate'][0]}
    for filename, digest in CORES.values():
        assert sha(MOD / 'cores' / filename) == digest
    assert selector['file'] in Path(f'/proc/{pids[0]}/maps').read_text()
    contract = json.loads((MOD / 'loader-contract.json').read_text())
    assert sha(MOD / 'dlls/main.dll') == contract['dll_sha256']
    assert all(sha(ROOT / p) == h for p, h in contract['sources'].items())
    OUT.mkdir()
    shutil.copyfile(__file__, OUT / 'recipe.py')
    log = (OUT / 'requests.jsonl').open('x')
    signatures = set()

    def send(value):
        log.write(json.dumps({'at_ns': time.monotonic_ns(), 'request': value}) + '\n'); log.flush()
        result = engine(value)
        log.write(json.dumps({'at_ns': time.monotonic_ns(), 'result': result}) + '\n'); log.flush()
        return result

    def get(target, name):
        return send({'op': 'get', 'target': target, 'property': name})

    def call(target, name, **fields):
        key = json.dumps(target, sort_keys=True), name
        if key not in signatures:
            send({'op': 'describe', 'target': target, 'function': name})
            signatures.add(key)
        return send({'op': 'call', 'target': target, 'function': name, 'args': fields}).get('ReturnValue')

    player = send({'op': 'player'})

    def resolve():
        assert send({'op': 'player'}) == player
        weapons = get(get(player['pawn'], 'WeaponsComponent'), 'EquippedWeapons')
        owners = {row['value']['name']: row['value'] for row in weapons['$map']
                  if not row['key']['TagName'].startswith('Weapon.Slot.Body.')}
        sidearm, = [row['value'] for row in weapons['$map'] if row['key']['TagName'] == 'Weapon.Slot.Sidearm']
        assert sidearm['class'].endswith('WP_Trebuchaxe_C')
        cls = send({'op': 'find', 'path': '/Script/Engine.MeshComponent'})
        components = {}
        for owner in owners.values():
            children = call(owner, 'K2_GetComponentsByClass', ComponentClass=cls)
            assert len(children) <= 64
            for child in children:
                components[child['name']] = child
        return weapons, owners, components, sidearm

    equipment, owners, components, sidearm = resolve()

    def flags():
        return dict(actors={name: get(obj, 'bHidden') for name, obj in owners.items()},
                    components={name: {'hidden': get(obj, 'bHiddenInGame'), 'visible': get(obj, 'bVisible')}
                                for name, obj in components.items()})

    original_flags = flags()
    assert get(sidearm, 'bHidden')
    preview_name, = [name for name in components if name.endswith('.SM_Projectile_Preview')]
    assert original_flags['components'][preview_name]['hidden']

    def geometry():
        return {k: call(player[t], f) for k, t, f in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('control_rotation', 'controller', 'GetControlRotation'), ('view_target', 'controller', 'GetViewTarget'))}

    def resources():
        health = get(player['pawn'], 'HealthComponent')
        assert not call(health, 'IsDeadOrDying')
        values = get(health, 'HealthSet')
        return {name: get(values, name) for name in ('Health', 'Resolve')}

    system = send({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})

    def graphics():
        return {name: call(system, 'GetConsoleVariableStringValue', VariableName=name)
                for name in ('t.MaxFPS', 'r.VSync', 'r.ScreenPercentage', 'r.DynamicRes.OperationMode')}

    start_geometry, start_resources, start_graphics = geometry(), resources(), graphics()
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
    window = lambda name: json.loads(subprocess.check_output(['hyprctl', name, '-j']))
    prior_window = window('activewindow')
    game_window, = [w for w in window('clients') if w['pid'] == pids[0] and w['class'] == 'steam_app_2584270']

    def focus(address):
        assert address.startswith('0x') and all(c in '0123456789abcdef' for c in address[2:])
        subprocess.run(['hyprctl', 'dispatch', 'hl.dsp.focus({window="address:' + address + '"})'],
                       check=True, capture_output=True)

    def keys():
        return send({'op': 'input.keys', 'target': player['controller'],
                     'keys': ['W', 'A', 'S', 'D', 'F', 'SpaceBar', 'LeftControl', 'LeftMouseButton', 'RightMouseButton']})

    expected_core = selector['file']

    def guard():
        assert processes() == pids and send({'op': 'player'}) == player
        assert json.loads((MOD / 'core.json').read_text()) == {'file': expected_core}
        assert expected_core in Path(f'/proc/{pids[0]}/maps').read_text()
        assert json.loads((MOD / 'state/state.json').read_text()) == expected_state
        current = describe()
        assert identity(current) == identity(original) and current['customize'] == original['customize']
        assert not current['busy'] and not current['expression']['performance'] and not current['idle_override']
        assert geometry_restored(start_geometry, geometry()) and resources() == start_resources
        assert graphics() == start_graphics and not any(keys().values())
        status = json.loads((MOD / 'runtime/status.json').read_text())
        assert not status['inventory']['active'] and status['astral']['active'] == status['astral']['cached'] == 0
        assert not status['animation']['engaged']
        anim = call(get(player['pawn'], 'Mesh'), 'GetAnimInstance')
        assert get(anim, 'Speed') <= .01 and not call(anim, 'IsAnyMontagePlaying')
        assert window('activewindow').get('address') == game_window['address']

    def restore_flags():
        assert set(components) == set(original_flags['components'])
        for name, obj in components.items():
            row = original_flags['components'][name]
            assert get(obj, 'bVisible') == row['visible']
            if get(obj, 'bHiddenInGame') != row['hidden']:
                call(obj, 'SetHiddenInGame', NewHidden=row['hidden'], bPropagateToChildren=False)
        assert flags() == original_flags

    def switch(label):
        nonlocal expected_core, owners, components, sidearm
        guard()
        filename = CORES[label][0]
        if filename == expected_core:
            return
        for _ in range(5):
            guard(); time.sleep(.2)
        old = expected_core
        report['reloads'].append({'from': old, 'to': filename, 'started_ns': time.time_ns()})
        atomic(OUT / 'result.json', report)
        atomic(MOD / 'core.json', {'file': filename})
        expected_core = filename
        wait_json(MOD / 'runtime/loader.json', lambda s: s.get('core') == filename, timeout=30)
        signatures.clear()
        deadline = time.monotonic() + 40
        while True:
            current = describe()
            if not current['busy'] and identity(current) == identity(original) and current['customize'] == original['customize']:
                break
            assert time.monotonic() < deadline, 'Appearance failed to settle after reload'
            time.sleep(.2)
        current_equipment, owners, components, sidearm = resolve()
        assert current_equipment == equipment
        time.sleep(1)
        guard()
        if label == 'candidate':
            restore_flags()
        report['reloads'][-1]['verified'] = True
        atomic(OUT / 'result.json', report)

    old_idle = state['animation_choices'][original['outfit']][original['variant']]['idle']
    assert old_idle in {row['id'] for row in original['idles']}
    report = dict(passed=False, cores=CORES, player=player, equipment=equipment, state_before=state,
                  geometry_before=start_geometry, resources_before=start_resources, graphics=start_graphics,
                  flags_before=original_flags, captures=[], reloads=[], events_per_capture=18,
                  scope='Matched 10-second episodes containing owner-visibility drift repair. MISC elapsed '
                        'phase and whole CSS tick/cadence; not exclusive per-event, cloth, GPU or display cost.')
    atomic(OUT / 'result.json', report)
    changed_idle = False
    try:
        focus(game_window['address'])
        assert json.loads((MOD / 'state/state.json').read_text()) == state
        expected_state['animation_choices'][original['outfit']][original['variant']]['idle'] = 'original'
        changed_idle = True
        css_capture.command('animation_choice', outfit=original['outfit'], variant=original['variant'], slot='idle', value='original')
        wait_json(MOD / 'runtime/status.json', lambda s: not s['animation']['engaged'])
        guard()
        for block, label in enumerate(('candidate', 'baseline', 'candidate')):
            switch(label)
            for repetition in range(3):
                guard()
                assert get(sidearm, 'bHidden')
                name = f'{block + 1}-{label}-{repetition + 1}'
                row = dict(name=name, core=expected_core, events=[], flags_before=flags())
                report['captures'].append(row)
                row['arm_sent_ns'] = time.monotonic_ns()
                acknowledgement = css_capture.command('frame_profile', seconds=10.)
                row['arm_ack_ns'] = time.monotonic_ns()
                rid = row['profile_id'] = acknowledgement['last_request']
                time.sleep(1.25)
                first = time.monotonic()
                for event in range(18):
                    delay = first + event * .4 - time.monotonic()
                    if delay > 0:
                        time.sleep(delay)
                    assert processes() == pids and get(sidearm, 'bHidden')
                    entry = {'sent_ns': time.monotonic_ns()}
                    call(sidearm, 'SetActorHiddenInGame', bNewHidden=False)
                    entry['ack_ns'] = time.monotonic_ns()
                    assert get(sidearm, 'bHidden'), 'CSS failed to restore hidden ownership'
                    entry['hidden_after'] = True
                    row['events'].append(entry)
                raw = wait_json(MOD / 'runtime/frame-profile.json', lambda s: s.get('id') == rid, timeout=20)
                row['completed_ns'] = time.monotonic_ns()
                assert raw['stop_reason'] == 'duration' and len(row['events']) == 18
                atomic(OUT / (name + '.json'), raw)
                row['raw_file'] = name + '.json'
                row['raw_sha256'] = sha(OUT / row['raw_file'])
                row['summary'] = summarize(raw)
                row['flags_after'] = flags()
                if label == 'candidate':
                    assert row['flags_after'] == original_flags
                guard()
                atomic(OUT / 'result.json', report)
                print(name, 'captured', row['summary']['frames'], 'frames', flush=True)
        report['captures_complete'] = True
    except Exception as exc:
        report['error'] = repr(exc)
        report['traceback'] = traceback.format_exc()
    finally:
        try:
            if changed_idle:
                switch('candidate')
                restore_flags()
                guard()
        except Exception as exc:
            report['flag_restoration_error'] = repr(exc)
            report['flag_restoration_traceback'] = traceback.format_exc()
        try:
            if changed_idle:
                assert json.loads((MOD / 'core.json').read_text()) == selector
                assert json.loads((MOD / 'state/state.json').read_text()) == expected_state
                assert identity(describe()) == identity(original)
                css_capture.command('animation_choice', outfit=original['outfit'], variant=original['variant'], slot='idle', value=old_idle)
                expected_state = state
                time.sleep(1)
            report['state_restored'] = json.loads((MOD / 'state/state.json').read_text()) == state
            report['selector_restored'] = json.loads((MOD / 'core.json').read_text()) == selector
            report['flags_restored'] = flags() == original_flags
            report['geometry_restored'] = geometry_restored(start_geometry, geometry())
            report['resources_restored'] = resources() == start_resources
            report['graphics_restored'] = graphics() == start_graphics
            report['cine_restored'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine
            report['keys_released'] = not any(keys().values())
            report['process_unchanged'] = processes() == pids
        except Exception as exc:
            report['restoration_error'] = repr(exc)
            report['restoration_traceback'] = traceback.format_exc()
        if window('activewindow').get('address') == game_window['address']:
            focus(prior_window['address'])
        report['focus_restored'] = window('activewindow').get('address') == prior_window['address']
        checks = ('state_restored', 'selector_restored', 'flags_restored', 'geometry_restored', 'resources_restored',
                  'graphics_restored', 'cine_restored', 'keys_released', 'process_unchanged', 'focus_restored')
        report['passed'] = bool(report.get('captures_complete') and not report.get('error') and
                              not report.get('flag_restoration_error') and
                              not report.get('restoration_error') and all(report.get(k) for k in checks))
        atomic(OUT / 'result.json', report)
        log.close()
    print(json.dumps({k: v for k, v in report.items() if k.endswith('restored') or k in ('passed', 'error', 'restoration_error')}))
    assert report['passed']


if __name__ == '__main__':
    main()

"""Activate the reviewed core using the established stationary reload procedure."""
import copy
import json
import math
from pathlib import Path
import shutil
import time
import traceback

from capture_eve_surface_variant import ROOT, describe, identity, processes, sha, atomic
from check_peony_live_presets import engine, request, GAME
from check_sidearm_aim_visibility import MOD, geometry_restored
from css import LOADER_ABI, copy_verified, wait_json
from css_live_snapshot import Probe
import css_capture

SOURCE = ROOT / 'work/three-outfit-rig-physics/runtime-review-20261015'
OUT = SOURCE / 'activation'


def main():
    assert not OUT.exists()
    manifest = json.loads((SOURCE / 'candidate.json').read_text())
    candidate = manifest['artifacts']['development']
    assert all(sha(ROOT / name) == digest for name, digest in manifest['sources'].items())
    assert sha(SOURCE / candidate['file']) == candidate['sha256']
    assert 'CSS_INVENTORY_DEV:BOOL=ON\n' in (ROOT / 'build/windows/CMakeCache.txt').read_text()
    contract = json.loads((MOD / 'loader-contract.json').read_text())
    assert contract['abi'] == LOADER_ABI and sha(MOD / 'dlls/main.dll') == contract['dll_sha256']
    assert all(sha(ROOT / name) == digest for name, digest in contract['sources'].items())
    old = json.loads((MOD / 'core.json').read_text())
    assert old == manifest['installed_unchanged']['selector']
    assert sha(MOD / 'cores' / old['file']) == manifest['installed_unchanged']['sha256']
    pids = processes()
    assert len(pids) == 1
    original = describe()
    assert identity(original)[1:] == ('eins0fx.seduxtress', 'peony')
    state = json.loads((MOD / 'state/state.json').read_text())
    expected_state = copy.deepcopy(state)
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
    buttons = {c['id']: c.get('enabled', True) for s in cine['sections'] for c in s['controls']}
    assert buttons.get('record') and buttons.get('capture')
    OUT.mkdir()
    shutil.copyfile(__file__, OUT / 'recipe.py')
    atomic(OUT / 'state-before.json', state)
    atomic(OUT / 'cine-before.json', cine)
    log = (OUT / 'requests.jsonl').open('x')
    probe = Probe(log)
    signatures = set()

    def send(value):
        log.write(json.dumps({'request': value, 'at_ns': time.monotonic_ns()}) + '\n'); log.flush()
        result = engine(value)
        log.write(json.dumps({'result': result, 'at_ns': time.monotonic_ns()}) + '\n'); log.flush()
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
        equipment = get(get(player['pawn'], 'WeaponsComponent'), 'EquippedWeapons')
        owners = {r['value']['name']: r['value'] for r in equipment['$map']
                  if not r['key']['TagName'].startswith('Weapon.Slot.Body.')}
        cls = send({'op': 'find', 'path': '/Script/Engine.MeshComponent'})
        components = {}
        for owner in owners.values():
            children = call(owner, 'K2_GetComponentsByClass', ComponentClass=cls)
            assert len(children) <= 64
            components.update({c['name']: c for c in children})
        return equipment, owners, components

    equipment, owners, components = resolve()

    def flags():
        return dict(actors={n: get(o, 'bHidden') for n, o in owners.items()},
                    components={n: dict(hidden=get(o, 'bHiddenInGame'), visible=get(o, 'bVisible'))
                                for n, o in components.items()})

    def geometry():
        return {k: call(player[t], f) for k, t, f in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('control_rotation', 'controller', 'GetControlRotation'), ('view_target', 'controller', 'GetViewTarget'))}

    def resources():
        health = get(player['pawn'], 'HealthComponent')
        assert not call(health, 'IsDeadOrDying')
        values = get(health, 'HealthSet')
        return {n: get(values, n) for n in ('Health', 'Resolve')}

    def keys():
        return send({'op': 'input.keys', 'target': player['controller'], 'keys':
                     ['W', 'A', 'S', 'D', 'F', 'Q', 'C', 'SpaceBar', 'LeftControl', 'LeftShift',
                      'LeftMouseButton', 'RightMouseButton']})

    def mapped(name):
        return any(line.rstrip().endswith('/' + name) for line in Path(f'/proc/{pids[0]}/maps').read_text().splitlines())

    start_flags, start_geometry, start_resources = flags(), geometry(), resources()
    old_idle = state['animation_choices'][original['outfit']][original['variant']]['idle']
    expected_core = old['file']
    initial_animation = json.loads((MOD / 'runtime/status.json').read_text())['animation']
    report = dict(passed=False, process=pids, candidate=candidate, old_core=old, player=player,
                  flags_before=start_flags, geometry_before=start_geometry, resources_before=start_resources,
                  animation_before=initial_animation, gameplay_input=False, camera_input=False)
    atomic(OUT / 'result.json', report)

    def guard(native_idle=False):
        assert processes() == pids and send({'op': 'player'}) == player
        assert json.loads((MOD / 'core.json').read_text()) == {'file': expected_core} and mapped(expected_core)
        assert json.loads((MOD / 'state/state.json').read_text()) == expected_state
        current = describe()
        assert identity(current) == identity(original) and current['customize'] == original['customize']
        assert not current['busy'] and not current['expression']['performance'] and not current['idle_override']
        assert geometry_restored(start_geometry, geometry()) and resources() == start_resources
        assert not any(keys().values())
        status = json.loads((MOD / 'runtime/status.json').read_text())
        assert not status['inventory']['active'] and status['astral']['active'] == status['astral']['cached'] == 0
        assert request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine
        for op in ('animation.control_rig_trial', 'animation.walk_trial'):
            assert not probe.send(op, action='status')['active']
        if native_idle:
            assert not status['animation']['engaged']
            anim = call(get(player['pawn'], 'Mesh'), 'GetAnimInstance')
            speed = get(anim, 'Speed')
            assert math.isfinite(speed) and abs(speed) <= .01 and not call(anim, 'IsAnyMontagePlaying')

    changed_idle = False
    try:
        guard()
        assert old_idle in {r['id'] for r in original['idles']}
        expected_state['animation_choices'][original['outfit']][original['variant']]['idle'] = 'original'
        changed_idle = True
        css_capture.command('animation_choice', outfit=original['outfit'], variant=original['variant'], slot='idle', value='original')
        wait_json(MOD / 'runtime/status.json', lambda s: not s['animation']['engaged'])
        for _ in range(5):
            guard(native_idle=True)
            time.sleep(.2)
        copy_verified(SOURCE / candidate['file'], MOD / 'cores' / candidate['file'])
        report['activation_started_ns'] = time.time_ns()
        atomic(OUT / 'result.json', report)
        atomic(MOD / 'core.json', {'file': candidate['file']})
        expected_core = candidate['file']
        report['loader'] = wait_json(MOD / 'runtime/loader.json', lambda s: s.get('core') == expected_core, timeout=30)
        signatures.clear()
        deadline = time.monotonic() + 40
        while True:
            current = describe()
            if not current['busy'] and identity(current) == identity(original) and current['customize'] == original['customize']:
                break
            assert time.monotonic() < deadline, 'Appearance did not settle after activation'
            time.sleep(.2)
        equipment_after, owners, components = resolve()
        assert equipment_after == equipment
        assert mapped(expected_core) and not mapped(old['file'])
        time.sleep(1)
        guard(native_idle=True)
        report['flags_after_reload'] = flags()
        assert set(components) == set(start_flags['components'])
        for name, obj in components.items():
            row = start_flags['components'][name]
            assert get(obj, 'bVisible') == row['visible']
            if get(obj, 'bHiddenInGame') != row['hidden']:
                call(obj, 'SetHiddenInGame', NewHidden=row['hidden'], bPropagateToChildren=False)
        assert flags() == start_flags
        report['activation_verified'] = True
        print('Reviewed core active; restoring original idle', flush=True)
    except Exception as exc:
        report['error'] = repr(exc)
        report['traceback'] = traceback.format_exc()
    finally:
        try:
            if changed_idle:
                guard(native_idle=True)
                css_capture.command('animation_choice', outfit=original['outfit'], variant=original['variant'], slot='idle', value=old_idle)
                expected_state = state
                wait_json(MOD / 'runtime/status.json', lambda s: s['animation']['engaged'] == initial_animation['engaged']
                          and s['animation']['shared']['idle'] == initial_animation['shared']['idle'], timeout=20)
            guard()
            report['state_restored'] = json.loads((MOD / 'state/state.json').read_text()) == state
            report['flags_restored'] = flags() == start_flags
            report['geometry_restored'] = geometry_restored(start_geometry, geometry())
            report['resources_restored'] = resources() == start_resources
            report['cine_restored'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine
            report['keys_released'] = not any(keys().values())
            report['animation_after'] = json.loads((MOD / 'runtime/status.json').read_text())['animation']
            report['process_unchanged'] = processes() == pids
            report['passed'] = bool(report.get('activation_verified') and not report.get('error')
                and all(report[k] for k in ('state_restored', 'flags_restored', 'geometry_restored',
                                            'resources_restored', 'cine_restored', 'keys_released', 'process_unchanged')))
        except Exception as exc:
            report['restoration_error'] = repr(exc)
            report['restoration_traceback'] = traceback.format_exc()
        atomic(OUT / 'state-after.json', json.loads((MOD / 'state/state.json').read_text()))
        atomic(OUT / 'result.json', report)
        log.close()
    print(json.dumps({k: v for k, v in report.items() if k.endswith('restored') or k in ('passed', 'error', 'restoration_error')}))
    assert report['passed']


if __name__ == '__main__':
    main()

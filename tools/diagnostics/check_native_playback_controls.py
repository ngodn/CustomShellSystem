"""Exercise native Genessa playback controls and a mapped parry, restoring Peony."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, describe, identity, settled, processes, sha, atomic
from check_peony_live_presets import customize, engine, request, GAME
from check_sidearm_aim_visibility import MOD, geometry_restored


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--modes', nargs='+', choices=('loop', 'pose'), default=['loop', 'pose'])
    args = parser.parse_args()
    out = args.output.resolve()
    original, pids = describe(), processes()
    assert not out.exists() and len(pids) == 1 and not original['busy']
    assert not original['idle_override'] and not original['expression']['performance']
    assert not any(engine({'op': 'menu.status'}).values())
    core = json.loads((MOD / 'core.json').read_text())
    assert sha(MOD / 'cores' / core['file']) == args.core_sha256
    assert core['file'] in Path(f'/proc/{pids[0]}/maps').read_text()
    state = json.loads((MOD / 'state/state.json').read_text())
    memory = state['remembered_custom']['css.original_shells/genessa']
    assert memory == state['remembered_custom']['css.original_shells']
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
    buttons = {c['id']: c.get('enabled', True) for s in cine['sections'] for c in s['controls']}
    assert buttons['record'] and buttons['capture']
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', state)
    atomic(out / 'cine-before.json', cine)
    report = dict(passed=False, original=original, core=core, core_sha256=args.core_sha256,
                  cases=[], scope='Native body controls in the current shell. No facial mapping or contact-quality claim.')
    log = (out / 'requests.jsonl').open('x')
    signatures = set()

    def logged(channel, value):
        log.write(json.dumps(dict(at=time.monotonic(), channel=channel, request=value)) + '\n'); log.flush()
        result = engine(value) if channel == 'engine' else customize(value)
        log.write(json.dumps(dict(at=time.monotonic(), result=result)) + '\n'); log.flush()
        return result

    def get(target, prop):
        return logged('engine', dict(op='get', target=target, property=prop))

    def call(target, function, **fields):
        key = target['$object'], function
        if key not in signatures:
            logged('engine', dict(op='describe', target=target, function=function))
            signatures.add(key)
        return logged('engine', dict(op='call', target=target, function=function, args=fields)).get('ReturnValue')

    def perf(**fields):
        return logged('customize', dict(action='performance', **fields))

    player = logged('engine', {'op': 'player'})
    mesh = get(player['pawn'], 'Mesh')
    health = get(player['pawn'], 'HealthComponent')
    health_set = get(health, 'HealthSet')
    weapons = get(player['pawn'], 'WeaponsComponent')
    input_component = call(player['pawn'], 'GetSpartaInputComponent')

    def geometry():
        return {k: call(player[t], f) for k, t, f in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('view_target', 'controller', 'GetViewTarget'), ('control_rotation', 'controller', 'GetControlRotation'))}

    def resources():
        return {name: get(health_set, name) for name in ('Health', 'Resolve')}

    def keys():
        return logged('engine', dict(op='input.keys', target=player['controller'],
                                    keys=['W', 'A', 'S', 'D', 'Q', 'F', 'SpaceBar', 'LeftMouseButton', 'RightMouseButton']))

    baseline, resource_before, equipment = geometry(), resources(), get(weapons, 'EquippedWeapons')
    report.update(player=player, geometry_before=baseline, resources_before=resource_before, equipment=equipment)
    target, expected, idle, token = identity(original), original['customize'], '', None

    def guard():
        current = describe()
        assert processes() == pids and logged('engine', {'op': 'player'}) == player
        assert identity(current) == target and current['customize'] == expected and current['idle_override'] == idle
        assert geometry_restored(baseline, geometry()) and resources() == resource_before
        assert get(weapons, 'EquippedWeapons') == equipment and not any(keys().values())
        assert not any(logged('engine', {'op': 'menu.status'}).values())

    def snapshot():
        p = perf(op='status')
        assert p['active'] and p['token'] == token and p['weight'] == 1
        montage = call(animation, 'GetCurrentActiveMontage')
        assert montage
        return dict(performance=p, montage=montage,
                    montage_seconds=call(animation, 'Montage_GetPosition', Montage=montage))

    def stop_owned():
        nonlocal token
        p = perf(op='status')
        if p['active']:
            assert token is not None and p['token'] == token, 'Preserving another performance owner'
            perf(op='stop', token=token)
        deadline = time.monotonic() + 8
        while perf(op='status')['active'] or call(animation, 'GetCurrentActiveMontage') is not None:
            assert time.monotonic() < deadline, 'Performance did not release its montage'
            time.sleep(.1)
        token = None

    atomic(out / 'result.json', report)
    try:
        guard()
        customize({'action': 'idle', 'value': 'eve'}); idle = 'eve'
        target, expected = (original['shell'], 'css.original_shells', 'genessa'), memory
        customize(dict(action='wear', outfit=target[1], variant=target[2], customize=expected, persist=False))
        settled(target, custom=expected)
        animation = call(mesh, 'GetAnimInstance')
        asset = report['native_asset'] = call(mesh, 'GetSkeletalMeshAsset')
        skeleton = report['native_skeleton'] = get(asset, 'Skeleton')
        assert skeleton['name'].endswith('/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton')
        assert call(animation, 'GetCurrentActiveMontage') is None and not perf(op='status')['active']
        guard()
        for clip, mode in (('css.eve.idle.default', 'loop'), ('css.eve.photo.confident', 'pose')):
            if mode not in args.modes:
                continue
            row = dict(clip=clip, mode=mode, passed=False)
            report['cases'].append(row)
            started = row['started'] = perf(op='start', id=clip, seconds=0., rate=1.)
            token = started['token']
            time.sleep(.5)
            row['initial'] = snapshot()
            row['tracks'] = get(row['initial']['montage'], 'SlotAnimTracks')
            assert '/NativeHuman/' in json.dumps(row['tracks'])
            duration = started['duration']
            assert duration > .1
            perf(op='seek', token=token, seconds=max(0., duration - .3))
            time.sleep(.7)
            row['boundary_a'] = snapshot()
            time.sleep(.5)
            row['boundary_b'] = snapshot()
            a, b = (row[k]['performance'] for k in ('boundary_a', 'boundary_b'))
            if mode == 'pose':
                assert a['phase'] == b['phase'] == 'holding' and a['seconds'] == b['seconds'] == duration
                assert abs(row['boundary_a']['montage_seconds'] - row['boundary_b']['montage_seconds']) < .001
            else:
                assert a['phase'] == b['phase'] == 'playing' and a['seconds'] < 1.5
                assert b['seconds'] > a['seconds']
            row['pause'] = perf(op='pause', token=token, paused=True)
            row['paused_a'] = snapshot()
            time.sleep(.6)
            row['paused_b'] = snapshot()
            a, b = row['paused_a'], row['paused_b']
            assert a['performance']['paused'] and b['performance']['paused']
            assert a['performance']['seconds'] == b['performance']['seconds']
            assert abs(a['montage_seconds'] - b['montage_seconds']) < .001
            perf(op='seek', token=token, seconds=.1)
            perf(op='pause', token=token, paused=False)
            time.sleep(.5)
            row['resumed'] = snapshot()
            assert row['resumed']['performance']['seconds'] > .1
            stop_owned(); guard()
            row['passed'] = True
            atomic(out / 'result.json', report)
            print(mode, 'passed', flush=True)
        folder = out / 'parry'
        command = [sys.executable, str(ROOT / 'tools/diagnostics/check_attack_interruption.py'),
                   '--output', str(folder), '--core-sha256', args.core_sha256,
                   '--native-body', '--action', 'parry']
        report['parry_command'] = command
        with (out / 'parry.log').open('x') as child_log:
            subprocess.run(command, stdout=child_log, stderr=subprocess.STDOUT, check=True)
        report['parry'] = json.loads((folder / 'result.json').read_text())
        assert report['parry']['passed']
        guard()
        report['checks_passed'] = True
    except Exception as error:
        report['error'] = repr(error)
    finally:
        try:
            if token is not None:
                stop_owned()
            guard()
            customize({'action': 'idle', 'value': original['idle_override']}); idle = original['idle_override']
            target, expected = identity(original), original['customize']
            customize(dict(action='wear', outfit=original['outfit'], variant=original['variant'], customize=expected, persist=True))
            settled(target, custom=expected)
            time.sleep(1.)
            guard()
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(out / 'state-after.json', after)
            checks = report['restoration'] = dict(saved_state=state == after,
                cine=cine == request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'],
                geometry=geometry_restored(baseline, geometry()), resources=resources() == resource_before,
                equipment=equipment == get(weapons, 'EquippedWeapons'), inputs=not any(keys().values()),
                input_blocks=not get(input_component, 'ActiveBlocks'), process=processes() == pids,
                appearance=identity(describe()) == identity(original), performance=not perf(op='status')['active'])
            report['passed'] = bool(report.get('checks_passed') and all(checks.values()))
        except Exception as error:
            report['restore_error'] = repr(error)
        atomic(out / 'result.json', report)
        log.close()
    print(json.dumps({k: v for k, v in report.items() if isinstance(v, (str, bool))}), flush=True)
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

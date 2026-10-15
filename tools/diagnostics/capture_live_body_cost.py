"""Compare body solver enabled/bypassed in the game, without changing saved controls."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time
import traceback

from capture_eve_surface_variant import ROOT, describe, identity, settled, processes, sha, atomic
from check_peony_live_presets import customize, engine, request, GAME
from check_sidearm_aim_visibility import MOD, geometry_restored
from sample_secondary_rigs import sample_secondary_rigs
from verify_capped_cost import finite, verify_capture

OUTFITS = [('eins0fx.seduxtress', 'peony'), ('eins0fx.unholygenessa', 'genessa'),
           ('eins0fx.commanderwhite', 'original')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--variants', nargs='+', choices=[v for o, v in OUTFITS], default=[v for o, v in OUTFITS])
    parser.add_argument('--pairs', nargs='+', type=int, choices=range(4), default=list(range(4)))
    args = parser.parse_args()
    assert args.pairs == sorted(set(args.pairs)), 'Pair indices must be unique and ordered'
    out = args.output.resolve()
    original, pids = describe(), processes()
    assert not out.exists() and len(pids) == 1 and not original['busy']
    assert not original['idle_override'] and not original['expression']['performance']
    core = json.loads((MOD / 'core.json').read_text())
    binary = MOD / 'cores' / core['file']
    assert sha(binary) == args.core_sha256 and core['file'] in Path(f'/proc/{pids[0]}/maps').read_text()
    state = json.loads((MOD / 'state/state.json').read_text())
    memories = state['remembered_custom']
    selected = [(o, v) for o, v in OUTFITS if v in args.variants]
    restore_variants = {o: next(k.split('/', 1)[1] for k, v in memories.items()
                               if k.startswith(o + '/') and v == memories[o]) for o, _ in selected}
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
    buttons = {c['id']: c.get('enabled', True) for s in cine['sections'] for c in s['controls']}
    assert buttons['record'] and buttons['capture']
    out.mkdir(parents=True)
    for name in ('capture_live_body_cost.py', 'sample_secondary_rigs.py'):
        shutil.copyfile(ROOT / 'tools/diagnostics' / name, out / name)
    shutil.copyfile(ROOT / 'tools/css_frame_profile.py', out / 'css_frame_profile.py')
    atomic(out / 'state-before.json', state)
    atomic(out / 'cine-before.json', cine)
    log = (out / 'requests.jsonl').open('x')

    def send(value):
        log.write(json.dumps(dict(at=time.monotonic(), request=value)) + '\n'); log.flush()
        result = engine(value)
        log.write(json.dumps(dict(at=time.monotonic(), result=result)) + '\n'); log.flush()
        return result

    def get(target, prop):
        return send(dict(op='get', target=target, property=prop))

    def call(target, function, **fields):
        send(dict(op='describe', target=target, function=function))
        return send(dict(op='call', target=target, function=function, args=fields)).get('ReturnValue')

    player = send({'op': 'player'})
    mesh = get(player['pawn'], 'Mesh')

    def body_enabled():
        instance = call(mesh, 'GetPostProcessInstance')
        if not instance:
            return None
        properties = send(dict(op='properties', target=instance, inherited=True))
        if not any(p['name'] == 'CSSBodyEnabled' for p in properties):
            return None
        return get(instance, 'CSSBodyEnabled')

    initial_body_enabled = body_enabled()
    health_set = get(get(player['pawn'], 'HealthComponent'), 'HealthSet')
    weapons = get(player['pawn'], 'WeaponsComponent')
    system = send(dict(op='find', path='/Script/Engine.Default__KismetSystemLibrary'))

    def context():
        return dict(geometry={k: call(player[t], f) for k, t, f in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('control_rotation', 'controller', 'GetControlRotation'), ('view_target', 'controller', 'GetViewTarget'))},
            resources={n: get(health_set, n) for n in ('Health', 'Resolve')},
            equipment=get(weapons, 'EquippedWeapons'),
            cvars={n: call(system, 'GetConsoleVariableStringValue', VariableName=n) for n in
                   ('t.MaxFPS', 'r.VSync', 'r.ScreenPercentage', 'r.DynamicRes.OperationMode')})

    baseline = context()
    settings = GAME.parents[2] / 'compatdata/2584270/pfx/drive_c/users/steamuser/AppData/Local/MortalShell2/Saved/Config/Windows/GameUserSettings.ini'
    settings_sha = sha(settings)
    window = lambda name: json.loads(subprocess.check_output(['hyprctl', name, '-j']))
    prior_window = window('activewindow')
    game_window = next(w for w in window('clients') if w['pid'] == pids[0] and w['class'] == 'steam_app_2584270')

    def focus(address):
        assert address.startswith('0x') and all(c in '0123456789abcdef' for c in address[2:])
        subprocess.run(['hyprctl', 'dispatch', 'hl.dsp.focus({window="address:' + address + '"})'], check=True, capture_output=True)

    target, expected, idle = identity(original), original['customize'], ''
    post, original_enable, changed = None, None, False
    report = dict(passed=False, original=original, core=core, core_sha256=args.core_sha256,
                  player=player, process=pids, context_before=baseline, settings_sha256=settings_sha,
                  requested_pairs=args.pairs,
                  cases=[], gameplay_input=False, scope='Whole-game frame intervals with only body solver bypass changed; not exclusive solver/GPU timing.')
    atomic(out / 'result.json', report)

    def guard(check_focus=True):
        value = describe()
        assert processes() == pids and send({'op': 'player'}) == player
        assert identity(value) == target and value['customize'] == expected and value['idle_override'] == idle
        assert not value['busy'] and not value['expression']['performance']
        assert not any(send({'op': 'menu.status'}).values()) and context() == baseline
        assert sha(settings) == settings_sha and json.loads((MOD / 'core.json').read_text()) == core
        assert not any(send(dict(op='input.keys', target=player['controller'], keys=[
            'W', 'A', 'S', 'D', 'Q', 'F', 'SpaceBar', 'LeftMouseButton', 'RightMouseButton'])).values())
        if check_focus:
            active_window = window('activewindow')
            if active_window.get('address') != game_window['address']:
                report['focus_lost'] = active_window
                raise RuntimeError('Game lost focus; capture stopped')

    def set_enabled(value):
        nonlocal changed
        assert call(mesh, 'GetPostProcessInstance') == post
        changed = True
        send(dict(op='set', target=post, property='CSSBodyEnabled', value=value))
        assert get(post, 'CSSBodyEnabled') is value

    def observation(enabled):
        assert get(post, 'CSSBodyEnabled') is enabled
        rigs = sample_secondary_rigs()
        assert rigs['post'] == post and finite(rigs)
        body = [r for r in rigs['rigs'] if r['object']['class'].rsplit('/', 1)[-1]
                in ('CR_Body.CR_Body_C', 'CR_BodyB3.CR_BodyB3_C')]
        assert len(body) == 1 and body[0]['values']['Enabled'] is enabled
        return dict(body=body[0], all=rigs)

    def restore_enable():
        nonlocal changed
        if changed:
            assert processes() == pids and call(mesh, 'GetPostProcessInstance') == post
            set_enabled(original_enable)
            time.sleep(.5)
            observation(original_enable)
            changed = False

    try:
        focus(game_window['address']); time.sleep(1.)
        focused = context(); time.sleep(2.)
        assert context() == focused, 'Player or view moving after focus settled'
        assert all(focused[k] == baseline[k] for k in ('resources', 'equipment', 'cvars'))
        report['context_before_focus'] = baseline
        baseline = focused
        report['context_before'] = baseline
        atomic(out / 'result.json', report)
        guard()
        customize(dict(action='idle', value='eve')); idle = 'eve'
        for outfit, variant in selected:
            guard()
            target, expected = (original['shell'], outfit, variant), memories[outfit + '/' + variant]
            customize(dict(action='wear', outfit=outfit, variant=variant, customize=expected, persist=False))
            settled(target, custom=expected)
            post = call(mesh, 'GetPostProcessInstance')
            properties = send(dict(op='properties', target=post, inherited=True))
            assert any(p['name'] == 'CSSBodyEnabled' for p in properties)
            original_enable = get(post, 'CSSBodyEnabled')
            assert original_enable is True
            preserved = {p['name']: get(post, p['name']) for p in properties
                         if p['name'].startswith('CSSBody') and p['name'] not in ('CSSBodyEnabled', 'CSSBodyResetEpoch', 'CSSBodyGeometryJson')}
            case = dict(outfit=outfit, variant=variant, post=post, preserved_inputs=preserved, captures=[])
            report['cases'].append(case)
            for pair in args.pairs:
                for enabled in ((False, True) if pair % 2 == 0 else (True, False)):
                    guard(); set_enabled(enabled); time.sleep(2.)
                    before = observation(enabled)
                    row = dict(pair=pair, enabled=enabled, rig_before=before)
                    case['captures'].append(row)
                    path = out / f'{variant}-pair{pair}-{"on" if enabled else "off"}.json'
                    command = [sys.executable, str(ROOT / 'tools/css_frame_profile.py'), '--seconds', '10',
                               '--core', str(binary), '--output', str(path)]
                    row.update(file=path.name, command=command)
                    atomic(out / 'result.json', report)
                    with path.with_suffix('.log').open('x') as capture_log:
                        subprocess.run(command, stdout=capture_log, stderr=subprocess.STDOUT, check=True)
                    data = json.loads(path.read_text()); verify_capture(data)
                    after = row['rig_after'] = observation(enabled)
                    a, b = before['body'], after['body']
                    assert a['object'] == b['object'] and a['values']['Limits'] == b['values']['Limits']
                    delta = row['body_steps'] = b['values']['TotalSteps'] - a['values']['TotalSteps']
                    assert delta > 0 if enabled else delta == 0
                    row['reset_delta'] = b['values']['ResetCount'] - a['values']['ResetCount']
                    assert row['reset_delta'] >= 0
                    assert {n: get(post, n) for n in preserved} == preserved
                    guard()
                    row['summary'] = data['summary']; row['passed'] = True
                    atomic(out / 'result.json', report)
                    print(variant, pair, enabled, 'steps', delta, 'Hz', round(data['summary']['engine_tick_rate_hz'], 2), flush=True)
            restore_enable()
            case['passed'] = True
            atomic(out / 'result.json', report)
        report['captures_complete'] = True
    except Exception as error:
        report['error'] = repr(error)
        report['traceback'] = traceback.format_exc()
    finally:
        try:
            restore_enable()
        except Exception as error:
            report['body_restore_error'] = repr(error)
        try:
            guard(check_focus=False)
            for outfit, variant in restore_variants.items():
                customize(dict(action='wear', outfit=outfit, variant=variant, customize=memories[outfit], persist=False))
                settled((original['shell'], outfit, variant), custom=memories[outfit])
            customize(dict(action='idle', value=original['idle_override'])); idle = original['idle_override']
            target, expected = identity(original), original['customize']
            customize(dict(action='wear', outfit=original['outfit'], variant=original['variant'], customize=expected, persist=True))
            settled(target, custom=expected); time.sleep(1.)
            guard(check_focus=False)
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(out / 'state-after.json', after)
            report['restoration'] = dict(state=after == state, context=context() == baseline,
                cine=request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine,
                process=processes() == pids, original_body_enabled=body_enabled() == initial_body_enabled)
        except Exception as error:
            report['restore_error'] = repr(error)
        if window('activewindow').get('address') == game_window['address'] and any(
                w['address'] == prior_window['address'] for w in window('clients')):
            focus(prior_window['address'])
        report['focus_restored'] = window('activewindow').get('address') == prior_window['address']
        report['passed'] = bool(report.get('captures_complete') and not report.get('restore_error') and
                               not report.get('body_restore_error') and
                               report.get('restoration') and all(report['restoration'].values()) and report['focus_restored'])
        atomic(out / 'result.json', report); log.close()
    if not report['passed']:
        print(json.dumps({k: v for k, v in report.items() if isinstance(v, (str, bool))}))
        raise SystemExit(1)


if __name__ == '__main__':
    main()

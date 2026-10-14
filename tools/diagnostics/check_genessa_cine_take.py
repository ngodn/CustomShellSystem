"""Run two stationary CINE portrait takes and restore the prior setup."""
import argparse
import json
from pathlib import Path
import shutil
import time

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, settled, sha, processes, css_capture
from check_peony_live_presets import customize, engine, request

CSS = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
CSSX = CSS.parent/'CSSX'
PRESET_ID = 'css-diagnostic-genessa-face-20261014'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--interrupt-at', type=float,
                        help='Change Genessa variant at this clip time and require a real interruption')
    args = parser.parse_args()
    if args.interrupt_at is not None and not 1 <= args.interrupt_at <= 10:
        parser.error('--interrupt-at must be between 1 and 10 seconds')
    out = args.output.resolve()
    installed = CSSX/'extensions/eins0fx.cine/presets'/f'{PRESET_ID}.cine.json'

    def api(**value):
        reply = request(GAME, value)
        if not reply['ok']:
            raise RuntimeError(reply['error'])
        return reply['result']

    def model():
        return api(op='model', extension='eins0fx.cine')

    def event(name, **fields):
        api(op='event', extension='eins0fx.cine', event={'id': name, **fields})
        value = model()
        if value['error']:
            raise RuntimeError(value['error'])
        return value

    def perf():
        return customize({'action': 'performance', 'op': 'status'})

    initial = model()
    original = describe()
    pids = processes()
    if out.exists() or installed.exists() or len(pids) != 1 or original['busy']:
        raise RuntimeError('Need one settled game and unused output/preset paths')
    controls = {c['id']: c.get('value') for s in initial['sections'] for c in s['controls']}
    enabled = {c['id']: c.get('enabled', True) for s in initial['sections'] for c in s['controls']}
    if not enabled['take'] or not enabled['exit'] or perf()['active']:
        raise RuntimeError('CINE must be armed between takes with no active performance')
    if controls['picture'] != 'game' or controls['breakdown'] != 'preset' or controls['expression'] != 'unchanged':
        raise RuntimeError('Fixture requires Game picture, preset breakdown and unchanged expression')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out/'recipe.py')
    state = json.loads((CSS/'state/state.json').read_text())
    memory = state['remembered_custom']['eins0fx.unholygenessa']
    memory_variant = next((v for v in ('eve', 'genessa')
                           if state['remembered_custom'].get('eins0fx.unholygenessa/'+v) == memory), None)
    if memory_variant is None:
        raise RuntimeError('No unchanged variant can restore the outfit-level remembered customization')
    cine_state = json.loads((CSSX/'state/eins0fx.cine.json').read_text())
    atomic(out/'state-before.json', state)
    atomic(out/'cine-before.json', cine_state)
    player = engine({'op': 'player'})

    def call(target, function):
        return engine({'op': 'call', 'target': target, 'function': function, 'args': {}})['ReturnValue']

    def geometry():
        return dict(position=call(player['pawn'], 'K2_GetActorLocation'),
                    rotation=call(player['pawn'], 'K2_GetActorRotation'),
                    control_rotation=call(player['controller'], 'GetControlRotation'),
                    view_target=call(player['controller'], 'GetViewTarget'))

    before_geometry = geometry()
    assert before_geometry['view_target'] == player['pawn'], 'Expected player-owned view before take'
    preset = dict(schema=1, id=PRESET_ID, name='CSS diagnostic Genessa face', kind='photo', hold=20,
                  reference='head', idle='eve', performance='css.eve.dance.irisout',
                  photos=[dict(name='Face', azimuth=15, distance=100, height=0, aim=-5, fov=30)])
    atomic(out/'preset.json', preset)
    report = dict(passed=False, original=original, process_before=pids, geometry_before=before_geometry,
                  cases=[], movement_input=False, camera='Temporary CINE photo take', started_ns=time.time_ns(),
                  interrupt_at=args.interrupt_at)
    target, expected = identity(original), original['customize']
    take_started = False
    installed_hash = None

    def guard():
        current = describe()
        if processes() != pids or identity(current) != target or current['customize'] != expected:
            raise RuntimeError('Concurrent appearance or process change')

    try:
        atomic(installed, preset)
        installed_hash = sha(installed)
        event('reload')
        event('preset', value=PRESET_ID)
        event('look', value='off')
        for variant in ('eve', 'genessa'):
            guard()
            customize({'action': 'idle', 'value': 'eve'})
            target = original['shell'], 'eins0fx.unholygenessa', variant
            customize({'action': 'wear', 'outfit': target[1], 'variant': variant, 'persist': False})
            active = settled(target)
            expected = active['customize']
            case = dict(variant=variant, samples=[], images=[], finished=False, interrupted=False)
            report['cases'].append(case)
            event('take')
            take_started = True
            started = time.monotonic()
            shot_at = [3., 10., 14.] if args.interrupt_at is None else []
            own_token = None
            switch_sent = False
            while time.monotonic()-started < 45:
                guard()
                m, p = model(), perf()
                case['samples'].append(dict(elapsed=time.monotonic()-started, status=m['status'], error=m['error'], performance=p))
                atomic(out/'result.json', report)
                if m['error']:
                    if switch_sent and m['error'].startswith('Performance interrupted: Appearance is changing'):
                        # CINE reads this reason from its stopped token before restoring the saved idle.
                        # The next RPC may already see that automatic idle's newer token.
                        if p['token'] == own_token:
                            assert not p['active'] and p['reason'] == 'Appearance is changing'
                        else:
                            assert p['token'] > own_token, 'Performance ownership moved backwards'
                        before_switch = case['before_switch']
                        assert before_switch['seconds'] < before_switch['duration']
                        case['interruption_message'] = m['error']
                        case['idle_resumed'] = p['token'] != own_token and p['active']
                        case['interrupted'] = True
                        break
                    raise RuntimeError(m['error'])
                if p['active'] and p['id'] == 'css.eve.dance.irisout':
                    if own_token is None:
                        own_token = p['token']
                    if p['token'] == own_token and shot_at and p['seconds'] >= shot_at[0]:
                        at = shot_at.pop(0)
                        css_capture.MEDIA = out
                        case['images'].append(str(css_capture.shot(f'{variant}-{at:g}')))
                    if (p['token'] == own_token and args.interrupt_at is not None
                            and not switch_sent and p['seconds'] >= args.interrupt_at):
                        case['before_switch'] = p
                        switched = 'genessa' if variant == 'eve' else 'eve'
                        target = original['shell'], 'eins0fx.unholygenessa', switched
                        customize({'action': 'wear', 'outfit': target[1], 'variant': switched, 'persist': False})
                        active = settled(target)
                        expected = active['customize']
                        case['after_switch'] = active
                        switch_sent = True
                if m['status'].startswith('Take finished.') and time.monotonic()-started > 4:
                    case['finished'] = True
                    take_started = False
                    break
                time.sleep(.35)
            if args.interrupt_at is None:
                assert case['finished'], 'CINE take did not finish normally'
                owned_samples = [s['performance'] for s in case['samples'] if s['performance']['token'] == own_token]
                assert own_token is not None and max(s['seconds'] for s in owned_samples) > 16
                assert any(not s['active'] for s in owned_samples), 'Natural clip end was not observed'
            else:
                assert case['interrupted'], 'Appearance change did not report an interruption'
                event('exit')
                event('enter')
                take_started = False
            case['token'] = own_token
            case['geometry_after'] = geometry()
            assert case['geometry_after'] == before_geometry, 'Player/view ownership changed'
            case['passed'] = True
            atomic(out/'result.json', report)
            print(variant, 'CINE appearance interruption pass' if args.interrupt_at is not None
                  else 'CINE take and natural performance end pass', flush=True)
    except Exception as error:
        report['error'] = repr(error)
        raise
    finally:
        try:
            guard()
            if take_started:
                # The supported exit action owns teardown if the take fails mid-flight.
                event('exit')
                event('enter')
            event('preset', value=controls['preset'])
            event('look', value=controls['look'])
            customize({'action': 'wear', 'outfit': 'eins0fx.unholygenessa', 'variant': memory_variant,
                       'customize': memory, 'persist': False})
            settled((original['shell'], 'eins0fx.unholygenessa', memory_variant), custom=memory)
            customize({'action': 'idle', 'value': original['idle_override']})
            customize({'action': 'wear', 'outfit': original['outfit'], 'variant': original['variant'],
                       'customize': original['customize'], 'persist': True})
            report['after_restore'] = settled(identity(original), custom=original['customize'])
            time.sleep(1.2)
            after = json.loads((CSS/'state/state.json').read_text())
            cine_after = json.loads((CSSX/'state/eins0fx.cine.json').read_text())
            atomic(out/'state-after.json', after)
            atomic(out/'cine-after.json', cine_after)
            report['saved_css_equal'] = state == after
            report['saved_cine_equal'] = cine_state == cine_after
            assert report['saved_css_equal'] and report['saved_cine_equal'], 'Saved state restoration differs'
            assert installed_hash and sha(installed) == installed_hash, 'Temporary preset changed independently'
            installed.unlink()
            event('reload')
            report['temporary_preset_removed'] = not installed.exists()
            report['geometry_after'] = geometry()
            assert report['geometry_after'] == before_geometry
            report['restored'] = True
        except Exception as error:
            report['restore_error'] = repr(error)
            raise
        finally:
            report['finished_ns'] = time.time_ns()
            report['process_after'] = processes()
            report['passed'] = not report.get('error') and not report.get('restore_error') and all(c.get('passed') for c in report['cases']) and len(report['cases']) == 2
            atomic(out/'result.json', report)
    print(json.dumps({k: report[k] for k in ('passed', 'restored', 'saved_css_equal', 'saved_cine_equal')}))


if __name__ == '__main__':
    main()

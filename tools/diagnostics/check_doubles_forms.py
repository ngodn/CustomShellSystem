"""Check normal doubles in both forms and modes, then restore user state."""
import argparse
import copy
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, describe, processes
from check_peony_live_presets import request, GAME
sys.path.insert(0, str(ROOT / 'tools'))
from css import atomic, wait_json
from css_live_snapshot import MOD, Probe

SHELLS = {'faithful': 'CharacterId.Player.Shell.Genessa',
          'stray': 'CharacterId.Player.Darkform.CorruptedGenessa'}


def restoration_path_ok(positions):
    # The observed form transition travels 551 cm and each clone action 443 cm.
    # Bound each recorded action as well as their accumulated distance.
    segments = [math.dist([a[k] for k in ('X','Y','Z')], [b[k] for k in ('X','Y','Z')])
                for a,b in zip(positions, positions[1:])]
    return bool(segments and all(math.isfinite(x) and x <= 600 for x in segments) and sum(segments) <= 1600)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    original, pids = describe(), processes()
    before = json.loads((MOD / 'state/state.json').read_text())
    if out.exists() or len(pids) != 1 or original['shell'] != SHELLS['faithful'] or original['busy']:
        raise RuntimeError('Need settled Faithful and a new output')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    expected = copy.deepcopy(before)
    report = dict(passed=False, cases=[], original=original, core_sha256=args.core_sha256)
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok']:
        raise RuntimeError('Cannot inspect CINE')
    atomic(out / 'cine-before.json', cine)
    with (out / 'requests.jsonl').open('x') as log:
        p = Probe(log)
        player = p.send('player')

        def call(target, function, **fields):
            p.send('describe', target=target, function=function)
            return p.call(target, function, **fields)

        def get(target, name):
            return p.send('get', target=target, property=name)

        def guard():
            current = p.send('player')
            if processes() != pids or any(current[k] != player[k] for k in ('pawn', 'controller')):
                raise RuntimeError('Player or process changed')
            if json.loads((MOD / 'state/state.json').read_text()) != expected:
                raise RuntimeError('Unexpected saved-state change')
            if any(p.send('input.keys', target=player['controller'], keys=['F','W','A','S','D','SpaceBar','LeftControl','LeftMouseButton','RightMouseButton']).values()):
                raise RuntimeError('Concurrent input detected')
            health = get(player['pawn'], 'HealthComponent')
            anim = call(get(player['pawn'], 'Mesh'), 'GetAnimInstance')
            if call(health, 'IsDeadOrDying') or get(anim, 'Speed') > .05 or call(anim, 'IsAnyMontagePlaying'):
                raise RuntimeError('Player is not stationary and alive')

        def geometry():
            return {k: call(t, f) for k, t, f in (
                ('position', player['pawn'], 'K2_GetActorLocation'),
                ('rotation', player['pawn'], 'K2_GetActorRotation'),
                ('view_target', player['controller'], 'GetViewTarget'),
                ('control_rotation', player['controller'], 'GetControlRotation'))}

        def command(action, **fields):
            value = dict(id=str(time.time_ns()), action=action, **fields)
            log.write(json.dumps({'command': value})+'\n'); log.flush()
            atomic(MOD / 'request.json', value)
            reply = wait_json(MOD / 'runtime/status.json', lambda s: s.get('last_request') == value['id'])
            log.write(json.dumps({'reply': reply})+'\n'); log.flush()
            return reply

        def wait_form(form):
            end = time.monotonic()+45
            while time.monotonic() < end:
                current = describe()
                if current['shell'] == SHELLS[form] and not current['busy']:
                    if any(current[k] != original[k] for k in ('outfit', 'variant', 'customize')):
                        raise RuntimeError('Mirrored appearance differs')
                    try:
                        guard()
                        return
                    except RuntimeError as error:
                        if str(error) != 'Player is not stationary and alive':
                            raise
                time.sleep(.3)
            raise TimeoutError('Form did not settle')

        guard()
        baseline = report['geometry_before'] = geometry()
        checkpoints = report['geometry_checkpoints'] = [baseline]
        try:
            if not expected['harbinger_mirror']:
                command('harbinger_mirror', value=True)
                expected['harbinger_mirror'] = True
                wait_json(MOD / 'state/state.json', lambda s: s == expected)
            for form, mode in (('faithful','default'), ('faithful','css'), ('stray','default'), ('stray','css')):
                guard()
                if p.send('player')['shell'] != SHELLS[form]:
                    command('genessa_form', mode=form)
                    wait_form(form)
                    checkpoints.append(geometry())
                command('doubles_mode', kind=form, mode=mode)
                expected['doubles'][form] = mode
                wait_json(MOD / 'state/state.json', lambda s: s == expected)
                guard()
                folder = out / (form+'-'+mode)
                argv = [sys.executable, str(ROOT / 'tools/diagnostics/check_doubles_summon.py'),
                        '--output', str(folder), '--core-sha256', args.core_sha256]
                if mode == 'css':
                    argv.append('--details')
                with (out / (form+'-'+mode+'.log')).open('x') as output:
                    result = subprocess.run(argv, stdout=output, stderr=subprocess.STDOUT)
                row = dict(form=form, mode=mode, evidence=str(folder), returncode=result.returncode)
                report['cases'].append(row)
                checkpoints.append(geometry())
                atomic(out / 'result.json', report)
                result.check_returncode()
        except Exception as error:
            report['error'] = repr(error)
        finally:
            try:
                guard()
                for form in ('faithful', 'stray'):
                    if expected['doubles'][form] != before['doubles'][form]:
                        command('doubles_mode', kind=form, mode=before['doubles'][form])
                        expected['doubles'][form] = before['doubles'][form]
                        wait_json(MOD / 'state/state.json', lambda s: s == expected)
                if p.send('player')['shell'] != SHELLS['faithful']:
                    command('genessa_form', mode='faithful')
                    wait_form('faithful')
                if expected['harbinger_mirror'] != before['harbinger_mirror']:
                    command('harbinger_mirror', value=before['harbinger_mirror'])
                    expected['harbinger_mirror'] = before['harbinger_mirror']
                    wait_json(MOD / 'state/state.json', lambda s: s == expected)
                guard()
                current = report['geometry_after_actions'] = geometry()
                checkpoints.append(current)
                if not restoration_path_ok([c['position'] for c in checkpoints]) or any(
                        c[k] != baseline[k] for c in checkpoints for k in ('view_target','control_rotation')):
                    raise RuntimeError('Unexpected player or view displacement; restoration stopped')
                if current != baseline:
                    time.sleep(.5)
                    guard()
                    if geometry() != current:
                        raise RuntimeError('Player moved; restoration stopped')
                    if not call(player['pawn'], 'K2_SetActorLocationAndRotation', NewLocation=baseline['position'],
                                NewRotation=baseline['rotation'], bSweep=False, bTeleport=True):
                        raise RuntimeError('Transform restoration failed')
                    time.sleep(.5)
                report['geometry_after'] = geometry()
                report['geometry_restored'] = all(report['geometry_after'][k] == baseline[k] for k in ('position','view_target','control_rotation')) and all(abs((report['geometry_after']['rotation'][a]-baseline['rotation'][a]+180)%360-180)<1e-8 for a in ('Pitch','Yaw','Roll'))
                after = json.loads((MOD / 'state/state.json').read_text())
                atomic(out / 'state-after.json', after)
                report['state_restored'] = after == before
                report['cine_restored'] = request(GAME, {'op':'model','extension':'eins0fx.cine'})['result'] == cine['result']
                report['appearance_restored'] = all(describe()[k] == original[k] for k in ('shell','outfit','variant','customize','idle_override'))
                report['passed'] = bool(len(report['cases']) == 4 and all(r['returncode'] == 0 for r in report['cases'])
                    and all(report[k] for k in ('geometry_restored','state_restored','cine_restored','appearance_restored')) and 'error' not in report)
            except Exception as error:
                report['restore_error'] = repr(error)
            atomic(out / 'result.json', report)
    print(json.dumps({k: report.get(k) for k in ('passed','cases','error','restore_error',
        'geometry_restored','state_restored','cine_restored','appearance_restored')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

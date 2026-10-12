"""Capture one temporary renderer toggle, then restore it. Python 3.14."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'extensions/core/tools'))
from cssx import GAME, atomic, processes, request, sha
import shot

CASES = {'reflections': 'r.Lumen.Reflections.Allow',
         'contact-shadows': 'r.ContactShadows',
         'virtual-shadows': 'r.Shadow.Virtual.Enable',
         'shadow-cache': 'r.Shadow.Virtual.Cache',
         'adaptive-shadow-rays': 'r.Shadow.Virtual.SMRT.AdaptiveRayCount',
         'directional-hard-shadows': 'r.Shadow.Virtual.SMRT.RayCountDirectional',
         'local-hard-shadows': 'r.Shadow.Virtual.SMRT.RayCountLocal'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=CASES, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or len(processes()) != 1:
        raise RuntimeError('Need one running game and a new output directory')
    args.output.mkdir(parents=True)
    shot.OUT = args.output
    result = dict(case=args.case, variable=CASES[args.case], completed=False,
                  restored=False, gameplay_input=False, script_sha256=sha(Path(__file__)),
                  started=datetime.now(timezone.utc).isoformat(), pids=processes(),
                  limitation='Sequential screenshots during ongoing animation; no animation phase matching or GPU timing.')
    changed = False

    def engine(value):
        reply = request(GAME, {'op': 'engine', 'request': value})
        if not reply['ok']:
            raise RuntimeError(reply)
        return reply['result']

    def call(target, name, **fields):
        return engine(dict(op='call', target=target, function=name, args=fields))

    def describe():
        return engine(dict(op='service.call', service='css.customize', version=1,
                           request={'action': 'describe'}))

    try:
        player = engine({'op': 'player'})
        result['player'] = player
        if not player.get('pawn') or not player.get('controller'):
            raise RuntimeError('No player loaded')
        result['appearance_before'] = describe()
        system = engine(dict(op='find', path='/Script/Engine.Default__KismetSystemLibrary'))
        camera = engine(dict(op='get', target=player['controller'], property='PlayerCameraManager'))
        result['signatures'] = {}
        for target, name in ((system, 'GetConsoleVariableStringValue'),
                             (system, 'ExecuteConsoleCommand'),
                             (camera, 'GetCameraLocation'), (camera, 'GetCameraRotation'),
                             (camera, 'GetFOVAngle'), (player['pawn'], 'K2_GetActorLocation')):
            result['signatures'][name] = engine(dict(op='describe', target=target, function=name))

        def view():
            return {name: call(target, name)['ReturnValue'] for target, name in (
                (camera, 'GetCameraLocation'), (camera, 'GetCameraRotation'),
                (camera, 'GetFOVAngle'), (player['pawn'], 'K2_GetActorLocation'))}

        def read():
            return call(system, 'GetConsoleVariableStringValue', VariableName=result['variable'])['ReturnValue']

        def write(value):
            call(system, 'ExecuteConsoleCommand', WorldContextObject=player['pawn'],
                 Command=result['variable'] + ' ' + value, SpecificPlayer=player['controller'])

        def capture(label):
            result[label + '_view_before'] = view()
            path = shot.shot(label)
            result[label + '_view_after'] = view()
            result[label + '_image'] = {'path': str(path), 'sha256': sha(path)}

        original = read()
        if not original.isdigit() or not 1 <= int(original) <= 64:
            raise RuntimeError('Expected enabled integer setting, got ' + repr(original))
        result['original'] = original
        capture('baseline')
        if engine({'op': 'player'}) != player or describe()['customize'] != result['appearance_before']['customize']:
            raise RuntimeError('Player or appearance changed before test')
        atomic(args.output / 'result.json', result)
        changed = True
        write('0')
        result['disabled_readback'] = read()
        if result['disabled_readback'] != '0':
            raise RuntimeError('Renderer toggle did not apply')
        time.sleep(2)
        capture('disabled')
        result['completed'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        try:
            if changed:
                write(original)
                result['restored_readback'] = read()
                result['restored'] = result['restored_readback'] == original
                if not result['restored']:
                    raise RuntimeError('Renderer setting failed to restore')
                time.sleep(2)
                capture('restored')
                result['appearance_after'] = describe()
                result['customize_unchanged'] = result['appearance_before']['customize'] == result['appearance_after']['customize']
                result['same_player'] = player == engine({'op': 'player'})
                result['same_process'] = result['pids'] == processes()
        except Exception as error:
            result['restoration_check_error'] = str(error)
            result['completed'] = False
            raise
        finally:
            result['finished'] = datetime.now(timezone.utc).isoformat()
            atomic(args.output / 'result.json', result)
    print(json.dumps({k: result.get(k) for k in ('case', 'completed', 'restored', 'customize_unchanged', 'same_player', 'same_process')}))


if __name__ == '__main__':
    main()

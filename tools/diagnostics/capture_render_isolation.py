"""Capture one temporary renderer setting, then restore it. Python 3.14."""
import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'extensions/core/tools'))
from cssx import GAME, atomic, processes, request, sha
import shot
from shadow_capture_phase import wait_phase

CASES = {'reflections': 'r.Lumen.Reflections.Allow',
         'contact-shadows': 'r.ContactShadows',
         'virtual-shadows': 'r.Shadow.Virtual.Enable',
         'shadow-cache': 'r.Shadow.Virtual.Cache',
         'adaptive-shadow-rays': 'r.Shadow.Virtual.SMRT.AdaptiveRayCount',
         'backface-shadow-culling': 'r.Shadow.Virtual.CullBackfacingPixels',
         'subsurface-shadow-angle': 'r.Shadow.Virtual.SubsurfaceShadowMinSourceAngle',
         'directional-hard-shadows': 'r.Shadow.Virtual.SMRT.RayCountDirectional',
         'directional-rays': 'r.Shadow.Virtual.SMRT.RayCountDirectional',
         'directional-samples': 'r.Shadow.Virtual.SMRT.SamplesPerRayDirectional',
         'directional-dither': 'r.Shadow.Virtual.SMRT.TexelDitherScaleDirectional',
         'directional-extrapolation': 'r.Shadow.Virtual.SMRT.ExtrapolateMaxSlopeDirectional',
         'third-party-upscaler': 'r.TemporalAA.Upscaler',
         'native-resolution': 'r.ScreenPercentage',
         'local-hard-shadows': 'r.Shadow.Virtual.SMRT.RayCountLocal'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=CASES, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--value', type=int, default=0)
    parser.add_argument('--phase', type=float, help='Wait for this time in the existing performance loop')
    args = parser.parse_args()
    if args.case == 'native-resolution':
        if args.value != 100:
            parser.error('The native-resolution control requires --value 100')
    elif not 0 <= args.value <= 64:
        parser.error('Renderer sampling values must be between 0 and 64')
    if args.case == 'directional-samples' and args.value == 0:
        parser.error('Samples per ray must stay above zero: UE-290828 can fully shadow scenes containing grooms')
    if args.value and args.case not in ('directional-rays', 'directional-samples', 'native-resolution'):
        parser.error('Nonzero trial values are limited to directional sampling cases')
    if args.output.exists() or len(processes()) != 1:
        raise RuntimeError('Need one running game and a new output directory')
    args.output.mkdir(parents=True)
    shot.OUT = args.output
    result = dict(case=args.case, variable=CASES[args.case], trial_value=str(args.value), completed=False,
                  restored=False, gameplay_input=False, script_sha256=sha(Path(__file__)),
                  started=datetime.now(timezone.utc).isoformat(), pids=processes(),
                  limitation='Sequential screenshots during ongoing animation; no animation phase matching or GPU timing. Console writes restore values but retain SetByConsole priority until restart.')
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
            if args.phase is not None:
                match = wait_phase(engine, args.phase, result.get('performance_identity'))
                result['performance_identity'] = match['identity']
                result[label + '_performance_phase'] = match
            path = shot.shot(label)
            result[label + '_view_after'] = view()
            result[label + '_image'] = {'path': str(path), 'sha256': sha(path)}

        original = read()
        if args.case == 'native-resolution':
            if not math.isfinite(float(original)) or not 25 <= float(original) < 100:
                raise RuntimeError('Expected a reduced positive screen percentage, got ' + repr(original))
        elif not original.isdigit() or not 1 <= int(original) <= 64:
            raise RuntimeError('Expected enabled integer setting, got ' + repr(original))
        result['original'] = original
        capture('baseline')
        if engine({'op': 'player'}) != player or describe()['customize'] != result['appearance_before']['customize']:
            raise RuntimeError('Player or appearance changed before test')
        atomic(args.output / 'result.json', result)
        changed = True
        write(result['trial_value'])
        result['trial_readback'] = read()
        if float(result['trial_readback']) != float(result['trial_value']):
            raise RuntimeError('Renderer toggle did not apply')
        time.sleep(2)
        capture('trial')
        result['completed'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        try:
            if changed:
                if processes() != result['pids'] or engine({'op': 'player'}) != player:
                    raise RuntimeError('Process or player changed; did not write into a different session')
                if float(read()) not in (float(original), float(result['trial_value'])):
                    raise RuntimeError('Renderer value changed concurrently; did not overwrite it')
                write(original)
                result['restored_readback'] = read()
                result['restored'] = result['restored_readback'] == original
                if not result['restored']:
                    raise RuntimeError('Renderer setting failed to restore')
                result['appearance_after'] = describe()
                result['customize_unchanged'] = result['appearance_before']['customize'] == result['appearance_after']['customize']
                result['same_player'] = player == engine({'op': 'player'})
                result['same_process'] = result['pids'] == processes()
                time.sleep(2)
                capture('restored')
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

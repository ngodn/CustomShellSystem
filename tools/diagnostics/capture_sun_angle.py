"""Compare one discovered directional light's source angle, then restore it."""
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
from shadow_capture_phase import wait_phase


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--discovery', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--angle', type=float, default=.5357)
    parser.add_argument('--phase', type=float, help='Wait for this time in the existing performance loop')
    args = parser.parse_args()
    if not 0 < args.angle <= 4:
        parser.error('Use a positive source angle no greater than 4 degrees')
    evidence = json.loads(args.discovery.read_text())
    lights = [m['values']['DirectionalLight'] for m in evidence['managers']
              if m.get('DirectionalLight_settings', {}).get('bVisible') and
              m['DirectionalLight_settings'].get('CastShadows')]
    if len(lights) != 1 or args.output.exists() or processes() != evidence['pids']:
        raise RuntimeError('Need one discovered visible light, unchanged process and new output')
    args.output.mkdir(parents=True)
    shot.OUT = args.output
    report = dict(completed=False, restored=False, gameplay_input=False,
                  started=datetime.now(timezone.utc).isoformat(), script_sha256=sha(Path(__file__)),
                  trial_angle=args.angle, pids=processes(),
                  limitation='Animation phases differ; no GPU timing. The source angle changes scene-wide shadow softness.')
    changed = False

    def engine(q):
        reply = request(GAME, dict(op='engine', request=q))
        if not reply['ok']:
            raise RuntimeError(reply)
        return reply['result']

    def call(target, name, **fields):
        return engine(dict(op='call', target=target, function=name, args=fields))

    def appearance():
        return engine(dict(op='service.call', service='css.customize', version=1,
                           request={'action': 'describe'}))

    try:
        player = engine({'op': 'player'})
        if player != evidence['player']:
            raise RuntimeError('Player changed since light discovery')
        report['player'] = player
        report['appearance_before'] = appearance()
        light = engine(dict(op='find', path=lights[0]['name'].split(' ', 1)[1]))
        if not light or light['class'] != 'Class /Script/Engine.DirectionalLightComponent':
            raise RuntimeError('Discovered light is no longer available')
        report['light'] = light
        report['setter'] = engine(dict(op='describe', target=light, function='SetLightSourceAngle'))
        camera = engine(dict(op='get', target=player['controller'], property='PlayerCameraManager'))
        getters = ((camera, 'GetCameraLocation'), (camera, 'GetCameraRotation'),
                   (camera, 'GetFOVAngle'), (player['pawn'], 'K2_GetActorLocation'))
        report['signatures'] = {name: engine(dict(op='describe', target=target, function=name))
                                for target, name in getters}

        def read():
            return engine(dict(op='get', target=light, property='LightSourceAngle'))

        def capture(label):
            report[label + '_view_before'] = {n: call(t, n)['ReturnValue'] for t, n in getters}
            if args.phase is not None:
                match = wait_phase(engine, args.phase, report.get('performance_identity'))
                report['performance_identity'] = match['identity']
                report[label + '_performance_phase'] = match
            p = shot.shot(label)
            report[label + '_image'] = dict(path=str(p), sha256=sha(p))
            report[label + '_view_after'] = {n: call(t, n)['ReturnValue'] for t, n in getters}
            report[label + '_angle'] = read()

        original = read()
        report['original_angle'] = original
        capture('baseline')
        atomic(args.output / 'result.json', report)
        if engine({'op': 'player'}) != player or read() != original:
            raise RuntimeError('Player or light changed before trial')
        changed = True
        call(light, 'SetLightSourceAngle', NewValue=args.angle)
        time.sleep(2)
        capture('trial')
        if abs(read() - args.angle) > 1e-5:
            raise RuntimeError('Source angle was not retained')
        report['completed'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        try:
            if changed:
                current = read()
                if current != original and abs(current - args.angle) > 1e-5:
                    raise RuntimeError('Light changed concurrently; did not overwrite its new angle')
                if current != original:
                    call(light, 'SetLightSourceAngle', NewValue=original)
                report['restored'] = read() == original
                report['same_player'] = player == engine({'op': 'player'})
                report['same_process'] = report['pids'] == processes()
                report['appearance_after'] = appearance()
                report['customize_unchanged'] = report['appearance_before']['customize'] == report['appearance_after']['customize']
                if not report['restored']:
                    raise RuntimeError('Original light angle did not restore')
                time.sleep(2)
                capture('restored')
        except Exception as error:
            report['restore_error'] = str(error)
            report['completed'] = False
            raise
        finally:
            atomic(args.output / 'result.json', report)
    print(json.dumps({k: report.get(k) for k in ('completed', 'restored', 'same_player', 'same_process', 'customize_unchanged')}))


if __name__ == '__main__':
    main()

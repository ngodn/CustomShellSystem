"""Compare the worn outfit with original Genessa through CSS, then restore it."""
import argparse
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'extensions/core/tools'))
from cssx import GAME, atomic, processes, request, sha
import shot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or len(processes()) != 1:
        raise RuntimeError('Need one game and a new output directory')
    args.output.mkdir(parents=True)
    shot.OUT = args.output
    report = dict(completed=False, pids=processes(), gameplay_input=False,
                  script_sha256=sha(Path(__file__)),
                  limitation='CSS remains active; appearances have different meshes and animation phases. This is not an unmodded-game control.')
    changed = False
    idle_changed = False

    def engine(q):
        r = request(GAME, dict(op='engine', request=q))
        if not r['ok']:
            raise RuntimeError(r)
        return r['result']

    def custom(action, **fields):
        return engine(dict(op='service.call', service='css.customize', version=1,
                           request=dict(action=action, **fields)))

    def call(target, name):
        return engine(dict(op='call', target=target, function=name, args={}))['ReturnValue']

    def settled(outfit, variant):
        deadline = time.monotonic() + 20
        while True:
            d = custom('describe')
            if not d['busy'] and (d['outfit'], d['variant']) == (outfit, variant):
                return d
            if time.monotonic() > deadline:
                raise RuntimeError('Appearance did not settle')
            time.sleep(.2)

    try:
        player = engine({'op': 'player'})
        report['player'] = player
        before = custom('describe')
        report['before'] = before
        if before['shell'] != 'CharacterId.Player.Shell.Genessa' or before['idle_override'] or before['busy']:
            raise RuntimeError('Need settled Faithful Genessa with no external idle override')
        state_path = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem/state/state.json'
        state_before = state_path.read_bytes()
        (args.output / 'state-before.json').write_bytes(state_before)
        camera = engine(dict(op='get', target=player['controller'], property='PlayerCameraManager'))
        getters = ((camera, 'GetCameraLocation'), (camera, 'GetCameraRotation'),
                   (camera, 'GetFOVAngle'), (player['pawn'], 'K2_GetActorLocation'))
        report['signatures'] = {name: engine(dict(op='describe', target=target, function=name))
                                for target, name in getters}

        def capture(label):
            report[label + '_view_before'] = {name: call(target, name) for target, name in getters}
            p = shot.shot(label)
            report[label + '_image'] = dict(path=str(p), sha256=sha(p))
            report[label + '_view_after'] = {name: call(target, name) for target, name in getters}
            report[label + '_appearance'] = custom('describe')
            report[label + '_mesh'] = engine(dict(op='get', target=player['pawn'], property='Mesh'))

        capture('baseline')
        if custom('describe')['customize'] != before['customize'] or engine({'op': 'player'}) != player:
            raise RuntimeError('Player or customization changed')
        atomic(args.output / 'result.json', report)
        changed = True
        custom('wear', outfit='css.original_shells', variant='genessa', persist=False)
        native = settled('css.original_shells', 'genessa')
        report['native_settled'] = native
        iris = [i['id'] for i in native['idles'] if i['name'] == 'Iris Out']
        if len(iris) == 1:
            idle_changed = True
            custom('idle', value=iris[0])
        time.sleep(4)
        capture('native')
        report['completed'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        try:
            if changed:
                current = custom('describe')
                if engine({'op': 'player'}) != player or (current['outfit'], current['variant']) != ('css.original_shells', 'genessa'):
                    raise RuntimeError('Player or outfit changed concurrently; did not overwrite it')
                if idle_changed:
                    if current['idle_override'] != iris[0]:
                        raise RuntimeError('Concurrent idle override; did not overwrite it')
                    custom('idle', value=before['idle_override'])
                custom('wear', outfit=before['outfit'], variant=before['variant'],
                       customize=before['customize'], persist=True)
                restored = settled(before['outfit'], before['variant'])
                time.sleep(4)
                capture('restored')
                report['customize_restored'] = restored['customize'] == before['customize']
                report['state_unchanged'] = state_before == state_path.read_bytes()
                report['same_player'] = player == engine({'op': 'player'})
                report['same_process'] = report['pids'] == processes()
        except Exception as error:
            report['restore_error'] = str(error)
            report['completed'] = False
            raise
        finally:
            atomic(args.output / 'result.json', report)
    print(json.dumps({k: report.get(k) for k in ('completed', 'customize_restored', 'state_unchanged', 'same_player', 'same_process')}))


if __name__ == '__main__':
    main()

"""Read Peony body presets and active animation inputs without changing motion."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'extensions/core/tools'))
from cssx import GAME, atomic, processes, request, sha

FIELDS = ('CSSBodyEnabled', 'CSSBodyUseRegionSettings', 'CSSBodyRegionFrequencies',
          'CSSBodyRegionDampingRatios', 'CSSBodyRegionMotionAmounts', 'CSSBodyRegionEnabled',
          'CSSBodyFrequency', 'CSSBodyDampingRatio', 'CSSBodyMotionAmount', 'CSSBodyResetEpoch')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise RuntimeError('Refusing to overwrite a capture')
    pids = processes()
    if len(pids) != 1:
        raise RuntimeError('Expected one running game; no request was sent')
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'

    def status():
        data = json.loads((mod / 'runtime/status.json').read_text())
        return {key: data.get(key) for key in ('applied', 'customize', 'animation', 'inventory', 'message')}

    def probe(**value):
        if value['op'] not in ('get', 'player', 'call'):
            raise ValueError('Unsupported capture operation')
        if value['op'] == 'call' and value['function'] not in ('GetPostProcessInstance', 'GetAnimInstance'):
            raise ValueError('Only animation-instance getters are allowed')
        reply = request(GAME, {'op': 'engine', 'request': value})
        if not reply['ok']:
            raise RuntimeError(reply['error'])
        return reply['result']

    initial = status()
    if initial['applied'] != 'eins0fx.seduxtress/peony':
        raise RuntimeError('Peony must be selected')
    result = dict(started=datetime.now(timezone.utc).isoformat(), initial=initial,
                  process_before=pids, samples=[], requests_finished=False,
                  appearance_writes=False, camera_input=False, recipe_sha256=sha(Path(__file__)))
    try:
        for index in range(3):
            if index:
                time.sleep(1)
            row = {'started': datetime.now(timezone.utc).isoformat(), 'status_before': status()}
            result['samples'].append(row)
            player = probe(op='player')
            row['player'] = player
            component = probe(op='get', target=player['pawn'], property='Mesh')
            row['mesh'] = probe(op='get', target=component, property='SkeletalMesh')
            row['component'] = component
            if '/PeonyV2/Meshes/' not in row['mesh']['name']:
                raise RuntimeError('Player no longer has Peony V2')
            row['main'] = probe(op='call', target=component, function='GetAnimInstance', args={})['ReturnValue']
            row['post'] = probe(op='call', target=component, function='GetPostProcessInstance', args={})['ReturnValue']
            row['fields'] = {}
            if row['post']:
                for field in FIELDS:
                    try:
                        row['fields'][field] = probe(op='get', target=row['post'], property=field)
                    except RuntimeError as error:
                        row['fields'][field] = {'unavailable': str(error)}
            row['status_after'] = status()
            row['player_after'] = probe(op='player')
            row['post_after'] = probe(op='call', target=component, function='GetPostProcessInstance', args={})['ReturnValue']
            row['selection_stable'] = (player == row['player_after'] and row['post'] == row['post_after'] and
                all(row['status_before'][key] == row['status_after'][key]
                    for key in ('applied', 'customize')))
        result['requests_finished'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        result['finished'] = datetime.now(timezone.utc).isoformat()
        result['process_after'] = processes()
        atomic(args.output, result)
    print(json.dumps({'requests_finished': True,
                      'stable_samples': sum(s['selection_stable'] for s in result['samples'])}))


if __name__ == '__main__':
    main()

"""Capture Peony's live material overrides through the installed CSSX dev channel."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'extensions/core/tools'))
from cssx import GAME, atomic, processes, request, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise RuntimeError('Refusing to overwrite a capture')
    pids = processes()
    if len(pids) != 1:
        raise RuntimeError('Expected one running game')
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'

    def status():
        value = json.loads((mod / 'runtime/status.json').read_text())
        return {k: value.get(k) for k in ('applied', 'customize', 'inventory', 'material_debug')}

    def probe(**value):
        if value['op'] not in ('player', 'get', 'call'):
            raise ValueError('Unsupported capture operation')
        if value['op'] == 'call' and value['function'] != 'GetMaterials':
            raise ValueError('Only GetMaterials is allowed')
        response = request(GAME, {'op': 'engine', 'request': value})
        if not response['ok']:
            raise RuntimeError(response['error'])
        return response['result']

    before = status()
    if before['applied'] != 'eins0fx.seduxtress/peony':
        raise RuntimeError('Peony is not selected')
    result = dict(started=datetime.now(timezone.utc).isoformat(), status_before=before,
                  process_before=pids, requests_finished=False, appearance_writes=False,
                  camera_input=False, materials=[], recipe_sha256=sha(Path(__file__)))
    try:
        player = probe(op='player')
        component = probe(op='get', target=player['pawn'], property='Mesh')
        mesh = probe(op='get', target=component, property='SkeletalMesh')
        if '/PeonyV2/Meshes/' not in mesh['name']:
            raise RuntimeError('The world component does not use Peony V2')
        materials = probe(op='call', target=component, function='GetMaterials', args={})['ReturnValue']
        if len(materials) != 46:
            raise RuntimeError('Unexpected Peony material slot count')
        result.update(player_before=player, component=component, mesh_before=mesh,
                      slots_before=materials)
        seen = set()

        def capture(instance, depth=0):
            if not instance or instance['$object'] in seen:
                return
            if depth > 8 or len(seen) >= 128:
                raise RuntimeError('Material parent traversal exceeded its bound')
            seen.add(instance['$object'])
            if instance['class'] not in ('Class /Script/Engine.MaterialInstanceDynamic',
                                         'Class /Script/Engine.MaterialInstanceConstant'):
                return
            row = {'object': instance}
            result['materials'].append(row)
            for name in ('Parent', 'ScalarParameterValues', 'VectorParameterValues',
                         'TextureParameterValues'):
                row[name] = probe(op='get', target=instance, property=name)
            capture(row['Parent'], depth + 1)

        for slot in (*range(20, 38), *range(41, 46)):
            capture(materials[slot])
        result['slots_after'] = probe(op='call', target=component, function='GetMaterials', args={})['ReturnValue']
        result['mesh_after'] = probe(op='get', target=component, property='SkeletalMesh')
        result['player_after'] = probe(op='player')
        result['status_after'] = status()
        result['process_after'] = processes()
        result['selection_stable'] = (
            pids == result['process_after'] and player == result['player_after'] and
            mesh == result['mesh_after'] and materials == result['slots_after'] and
            all(before[key] == result['status_after'][key] for key in ('applied', 'customize')))
        result['requests_finished'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        result['finished'] = datetime.now(timezone.utc).isoformat()
        atomic(args.output, result)
    print(json.dumps({k: result[k] for k in ('requests_finished', 'selection_stable')}
                     | {'captured_instances': len(result['materials'])}))


if __name__ == '__main__':
    main()

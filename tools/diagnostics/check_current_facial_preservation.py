"""Compare installed normal/facial mesh digests and decoded cloth properties.

Python 3.14. Genessa's authored mouth geometry is checked separately.
"""
import argparse
import copy
import json
from pathlib import Path

from check_import_cooked_mesh import sha


def local_identity(index, exports, package, seen=()):
    if index in seen or not 0 <= index < len(exports):
        raise ValueError('Invalid local reference or outer cycle')
    obj = exports[index]
    short = package.rsplit('/', 1)[1]
    result = dict(type=obj['Type'], name='<mesh>' if obj['Name'] == short else obj['Name'])
    outer = obj.get('Outer')
    if outer:
        path = outer['ObjectPath']
        if not path.startswith(package+'.'):
            raise ValueError('Unexpected external outer')
        result['outer'] = local_identity(int(path[len(package)+1:]), exports, package, (*seen, index))
    return result


def normalize(value, exports, package):
    if isinstance(value, list):
        return [normalize(v, exports, package) for v in value]
    if isinstance(value, dict):
        result = {k: normalize(v, exports, package) for k, v in value.items()}
        path = value.get('ObjectPath', '')
        if path.startswith(package+'.'):
            result['ObjectPath'] = local_identity(int(path[len(package)+1:]), exports, package)
        return result
    if isinstance(value, str):
        short = package.rsplit('/', 1)[1]
        value = value.replace("'"+short+":", "'<mesh>:")
        value = value.replace("SkeletalMesh'"+short+"'", "SkeletalMesh'<mesh>'")
    return value


def compare(inventory, raw):
    rows = []
    cloth_types = {'ClothingAssetCommon', 'ChaosClothConfig', 'ChaosClothSharedSimConfig'}
    for variant in inventory['variants']:
        paths = [variant['meshes'][k] for k in ('normal', 'facial')]
        normal, face = [inventory['meshes'][p] for p in paths]
        meshes = [next(e for e in raw[p] if e['Type'] == 'SkeletalMesh') for p in paths]
        a, b = normal['digest'], face['digest']
        if a['reference_skeleton_sha256'] != b['reference_skeleton_sha256']:
            raise ValueError('Changed mesh bind reference')
        if any(b['morphs'].get(name) != value for name, value in a['morphs'].items()):
            raise ValueError('Original morph missing or changed')
        cw = variant['outfit'] == 'eins0fx.commanderwhite'
        genessa = variant['outfit'] == 'eins0fx.unholygenessa'
        added = len(b['morphs'])-len(a['morphs'])
        if added != (190 if cw else 115):
            raise ValueError('Unexpected added morph count')
        for key in ('Skeleton', 'PhysicsAsset'):
            if normal['references'][key] != face['references'][key]:
                raise ValueError('Changed '+key)
        graph_equal = normal['references']['PostProcessAnimBlueprint'] == face['references']['PostProcessAnimBlueprint']
        if not graph_equal and not cw:
            raise ValueError('Unexpected facial post-process replacement')
        if cw and [m['references']['PostProcessAnimBlueprint'] for m in (normal, face)] != [
            '/Game/CSS/eins0fx/CommanderWhite/Rig/ABP_NormalBelt20',
            '/Game/CSS/eins0fx/CommanderWhite/Rig/ABP_FacialBelt20']:
            raise ValueError('CW no longer uses the checked Belt20 graph pair')
        lod_equal = a['lods'] == b['lods']
        if not genessa and not lod_equal:
            raise ValueError('Unexpected base render-buffer change')
        cloth = [normalize([e for e in raw[p] if e['Type'] in cloth_types], raw[p], p) for p in paths]
        if cloth[0] != cloth[1]:
            raise ValueError('Decoded cloth properties changed')
        materials = [m['SkeletalMaterials'] for m in meshes]
        changes = []
        if genessa:
            if materials[0] != materials[1][:-1] or materials[1][-1]['MaterialSlotName'] != 'UG_Mouth':
                raise ValueError('Genessa changes extend beyond the appended mouth material')
        elif cw:
            expected = copy.deepcopy(materials[0])
            for slot in (8, 10, 45):
                expected[slot]['Material'] = dict(
                    ObjectName=f"MaterialInstanceConstant'MI_Optical_{slot}'",
                    ObjectPath=f'/Game/CSS/eins0fx/CommanderWhite/Faces/Optical/MI_Optical_{slot}.0')
            if materials[1] != expected:
                raise ValueError('CW changes extend beyond the three accepted optical materials')
            changes = [8, 10, 45]
        elif materials[0] != materials[1]:
            raise ValueError('Unexpected material-slot change')
        rows.append(dict(outfit=variant['outfit'], variant=variant['variant'], meshes=variant['meshes'],
            bind_equal=True, original_morphs_preserved=len(a['morphs']), added_morphs=added,
            render_digests_equal=lod_equal, decoded_cloth_exports_equal=len(cloth[0]),
            cloth_assets=sum(e['Type'] == 'ClothingAssetCommon' for e in raw[paths[0]]),
            graph_equal=graph_equal, optical_material_slots=changes, appended_mouth_material=genessa))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inventory = json.loads(args.inventory.read_text())
    raw_path = Path(inventory['raw_readback']['path'])
    if sha(raw_path) != inventory['raw_readback']['sha256']:
        raise ValueError('Raw readback changed')
    inputs = {str(args.inventory): sha(args.inventory), str(raw_path): sha(raw_path),
              str(Path(__file__)): sha(Path(__file__)), **inventory['protected']}
    for path, digest in inputs.items():
        if sha(Path(path)) != digest:
            raise ValueError('Pinned input changed: '+path)
    rows = compare(inventory, json.loads(raw_path.read_text()))
    for path, digest in inputs.items():
        if sha(Path(path)) != digest:
            raise ValueError('Input changed during comparison: '+path)
    args.output.write_text(json.dumps(dict(passed=True, rows=rows, inputs=inputs,
        scope='Decoded bind, original morph and selected binding preservation for ten pairs; '
              'exact render digests for eight pairs; serialized cloth properties for all pairs. '
              'Genessa mouth geometry and morph authoring use a separate comparison. '
              'No bounds, Nanite payload, omitted serializer fields, simulation, graph execution or gameplay claim.'), indent=2)+'\n')
    print(f'Checked {len(rows)} normal/facial pairs.')


if __name__ == '__main__':
    main()

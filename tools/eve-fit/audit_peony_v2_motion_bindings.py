"""Join Peony V2's cooked skin weights to its installed solver bindings.

Host Python 3.14. This reads retained cook results and never controls the game.
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'diagnostics'))
from audit_secondary_bindings import compare_adapter, graph, mesh_summary, package, rig_summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--character', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs = {}

    def read_bytes(path, expected=None):
        raw = path.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if expected is not None and digest != expected:
            raise ValueError('Changed input: ' + str(path))
        inputs[str(path.resolve())] = digest
        return raw

    def read(path, expected=None):
        return json.loads(read_bytes(path, expected))

    baseline = read(args.baseline)
    if not baseline['passed']:
        raise ValueError('Secondary baseline did not pass')
    paths = [Path(p) for p in baseline['inputs'] if p.endswith('/eve-final-container1/decoded.json')]
    if len(paths) != 1:
        raise ValueError('Ambiguous accepted Eve readback')
    data = read(paths[0], baseline['inputs'][str(paths[0])])
    proof = read(args.character / 'cooked-readback1/verified.json')
    if not proof['passed']:
        raise ValueError('Character cook did not pass')
    character = read(args.character / 'cooked-readback1/decoded.json', proof['decoded_sha256'])
    library = read(args.catalog)['catalog']['animation_library']
    body_mesh = '/Game/CSS/eins0fx/EveSB/PeonyV2/Meshes/SK_PeonyV2'
    face_mesh = body_mesh + '_Face'
    meshes = {p: mesh_summary(character, p) for p in (body_mesh, face_mesh)}
    if meshes[body_mesh] != meshes[face_mesh]:
        raise ValueError('Normal and facial secondary bindings differ')
    source = meshes[body_mesh]['post_process']
    adapters = [a for a in library['adapters']
                if a['source_outfit'] == 'eins0fx.seduxtress' and package(a['source_class']) == source]
    if len(adapters) != 1:
        raise ValueError('Ambiguous Peony playback adapter')
    target = package(adapters[0]['target_class'])
    normal, playback = graph(data, source), graph(data, target)
    failures = compare_adapter(normal, playback)
    if failures:
        raise ValueError('; '.join(failures))
    rigs = {r['rig']: rig_summary(data, r['rig']) for r in normal['rigs']}
    controls = read(args.character / 'character-bindings.json')
    expected_bones = controls['meshes'][0]['bind_pose']
    sections = None
    base_streams = None
    for name in ('SK_PeonyV2', 'SK_PeonyV2_Face'):
        folder = args.character / 'cooked-character1' / name
        audit = read(folder / 'skin-audit.json')
        if audit['bones'] != expected_bones:
            # Editor JSON and the binary cook can have different float precision.
            if [(b['name'], b['parent']) for b in audit['bones']] != [
                    (b['name'], b['parent']) for b in expected_bones]:
                raise ValueError('Cooked hierarchy differs from the verified binding')
        streams = {r['name']: read_bytes(folder / r['name'], r['sha256']) for r in audit['files']}
        if base_streams is not None and streams != base_streams:
            raise ValueError('Normal and facial cooked skin streams differ')
        base_streams = streams
        layout = struct.Struct('<3f' + 'HHf' * audit['max_influences'])
        if layout.size != audit['vertex_stride']:
            raise ValueError('Unexpected cooked skin layout')
        vertices = list(layout.iter_unpack(streams['skin-vertices.bin']))
        indices = [v[0] for v in struct.iter_unpack('<I', streams['skin-indices.bin'])]
        if len(vertices) != audit['vertex_count'] or len(indices) != audit['index_count']:
            raise ValueError('Cooked stream counts differ')
        bones = audit['bones']
        names = {bone['name'] for bone in bones}
        for rig in rigs.values():
            if not set(rig['defaults'].get('BoneNames', [])) <= names:
                raise ValueError('A named solver target is absent from the cooked skeleton')
        ancestors = []
        for i, bone in enumerate(bones):
            parent = bone['parent']
            if parent < -1 or parent >= i:
                raise ValueError('Invalid cooked hierarchy')
            ancestors.append({bone['name']} | (ancestors[parent] if parent >= 0 else set()))
        rows = []
        for section in audit['sections']:
            slot = section['MaterialIndex']
            start = section['FirstIndex']
            points = set(indices[start:start + section['NumFaces'] * 3])
            weighted = {}
            for point in points:
                vertex = vertices[point]
                for offset in range(3, len(vertex), 3):
                    bone, raw, weight = vertex[offset:offset + 3]
                    if weight <= 0:
                        continue
                    if bone >= len(bones):
                        raise ValueError('Cooked influence is outside the skeleton')
                    stats = weighted.setdefault(bone, {'vertices': 0, 'weight_sum': 0.0})
                    stats['vertices'] += 1
                    stats['weight_sum'] += weight
            owners = []
            for path, rig in rigs.items():
                targets = set(rig['defaults'].get('BoneNames', []))
                direct = {bones[b]['name']: w for b, w in weighted.items() if bones[b]['name'] in targets}
                descendants = {bones[b]['name']: w for b, w in weighted.items()
                               if bones[b]['name'] not in targets and ancestors[b] & targets}
                if direct or descendants:
                    owners.append({'rig': path, 'direct': direct, 'potential_descendants': descendants})
            rows.append({'slot': slot, 'material': audit['material_slots'][slot],
                         'vertices': len(points), 'owners': owners})
        if sections is not None and rows != sections:
            raise ValueError('Normal and facial skin influence coverage differs')
        sections = rows
    args.output.parent.mkdir(parents=True, exist_ok=True)
    read_bytes(Path(__file__).resolve())
    read_bytes(Path(__file__).resolve().parents[1] / 'diagnostics/audit_secondary_bindings.py')
    result = {'passed': True, 'inputs': inputs, 'meshes': meshes, 'source_graph': source,
              'playback_graph': target, 'writer_orders': normal['writer_orders'],
              'rigs': rigs, 'sections': sections,
              'limitations': 'Cooked bindings and skin influence coverage only. Descendant influence '
              'does not prove propagation. No live solver, cloth, contact or performance acceptance.'}
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'passed': True, 'meshes': len(meshes), 'sections': len(sections),
                      'cloth_assets': len(meshes[body_mesh]['cloth']), 'rigs': len(rigs)}))


if __name__ == '__main__':
    main()

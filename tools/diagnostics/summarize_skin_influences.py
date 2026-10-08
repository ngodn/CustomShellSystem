"""Join verified editor skin weights with the cooked region-owner inventory."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--owners', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    inputs = []

    def read(path, expected=None):
        raw = path.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if expected and digest != expected:
            raise ValueError('Changed input: ' + str(path))
        inputs.append(dict(path=str(path), sha256=digest))
        return json.loads(raw)

    capture, owners = read(args.capture), read(args.owners)
    if not capture['passed'] or not owners['passed']:
        raise ValueError('Input audit did not pass')
    if {r['package'] for r in capture['meshes']} != set(owners['meshes']):
        raise ValueError('Mesh coverage differs from the owner inventory')
    meshes = []
    for row in capture['meshes']:
        mesh = read(Path(row['report']), row['report_sha256'])
        owner = owners['meshes'][row['package']]
        graph = owners['graphs'][owner['post_process']]
        bones = mesh['bones']
        by_name = {b['name']: i for i, b in enumerate(bones)}
        if len(by_name) != len(bones):
            raise ValueError('Duplicate bone name')
        ancestors = []
        for index, bone in enumerate(bones):
            parent = bone['parent']
            if parent < -1 or parent >= index:
                raise ValueError('Invalid hierarchy')
            ancestors.append({bone['name']} | (ancestors[parent] if parent >= 0 else set()))
        regions = {r['rig']: set(owners['rigs'][r['rig']]['defaults'].get('BoneNames', [])) for r in graph['rigs']}
        # Explicit ModifyBone writers are separate from the rigs' named arrays.
        for order in graph['writer_orders']:
            for writer in order:
                if writer.startswith('bone:'):
                    regions[writer] = {writer.removeprefix('bone:')}
        summaries = []
        for region, targets in regions.items():
            if not targets:
                continue
            if not targets <= by_name.keys():
                raise ValueError('A region target is absent from the mesh')
            descendants = {bones[i]['name'] for i, chain in enumerate(ancestors) if chain & targets}
            sections = []
            for lod_index, lod in enumerate(mesh['render_lods']):
                for index, section in enumerate(lod['sections']):
                    weighted = section['weighted_bones']
                    direct = {k: v for k, v in weighted.items() if k in targets}
                    inherited = {k: v for k, v in weighted.items() if k in descendants - targets}
                    if direct or inherited:
                        sections.append(dict(lod=lod_index, section=index, slot=section['material_slot'],
                            cloth_asset_index=section['cloth_asset_index'], direct=direct,
                            potential_descendant_influences=inherited))
            summaries.append(dict(owner=region, targets=sorted(targets), sections=sections))
        meshes.append(dict(package=row['package'], regions=summaries))
    result = dict(meshes=meshes, inputs=inputs,
        scope='Direct render skin influences and potential descendant coverage. Does not prove rig propagation, cloth render transfer, dynamic contact or visible quality. Hand literals are covered by the separate hand audit.')
    with args.output.open('x') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')


if __name__ == '__main__':
    main()

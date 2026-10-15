"""Prepare isolated NPC meshes for the existing UE 5.6.1 importer. Python 3.14.

Keeps the source bind pose and surface. Zhirelle's left leg receives the human
animation names; aliases retain the original names for physics references.
This prepares an animation candidate, not a validated catalog entry.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path

import numpy as np


ALIASES = {
    'unrealHip1_L': 'thigh_l',
    'unrealKnee1_L': 'calf_l',
    'unrealAnkle1_L': 'foot_l',
    'unrealToes1_L': 'ball_l',
}
SOURCES = {
    'Zhirelle': '/Game/Sparta/Characters/NPCs/ShellKeeper/Art/Mesh/SK_ShellKeeper.SK_ShellKeeper',
    'Merrick': '/Game/Sparta/Characters/NPCs/Shopkeeper/Art/Mesh/MH/SK_Shopkeeper_MH.SK_Shopkeeper_MH',
}


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def prepare(source, name, output):
    if output.exists() or output.with_suffix('.recipe.json').exists():
        raise FileExistsError(output)
    audit = json.loads((source / 'skin-audit.json').read_text())
    surface = json.loads((source / 'skin-surface.json').read_text())
    if audit['asset'] != SOURCES[name] or audit['schema'] != 1 or surface['schema'] != 1:
        raise ValueError('Unexpected source mesh or export schema')
    files = [*audit['files'], surface['file']]
    for record in files:
        path = source / record['name']
        if sha(path) != record['sha256'] or path.stat().st_size != record['bytes']:
            raise ValueError(f'Source stream changed: {path}')
    count, maximum = audit['vertex_count'], audit['max_influences']
    if not 1 <= maximum <= 8:
        raise ValueError('Importer supports at most eight influences')
    dtype = np.dtype([('position', '<f4', 3), ('weights', [
        ('bone', '<u2'), ('raw', '<u2'), ('weight', '<f4')], maximum)])
    if dtype.itemsize != audit['vertex_stride']:
        raise ValueError('Unexpected skin stride')
    vertices = np.fromfile(source / 'skin-vertices.bin', dtype=dtype)
    indices = np.fromfile(source / 'skin-indices.bin', dtype='<u4')
    uv_count = surface['uv_channels']
    color_count = len(surface['color_channels'])
    surface_type = np.dtype([('normal', '<f4', 4), ('tangent', '<f4', 4),
                             ('uv', '<f4', (uv_count, 2)), ('color', 'u1', (color_count, 4))])
    if surface_type.itemsize != surface['stride'] or not 1 <= uv_count <= 4 or color_count > 1:
        raise ValueError('Unsupported surface layout')
    channels = np.fromfile(source / 'skin-surface.bin', dtype=surface_type)
    if len(vertices) != count or len(channels) != count or len(indices) != audit['index_count']:
        raise ValueError('Incomplete mesh streams')
    if len(indices) % 3 or np.any(indices >= count):
        raise ValueError('Invalid triangle indices')
    bones = copy.deepcopy(audit['bones'])
    names = {b['name']: i for i, b in enumerate(bones)}
    if len(names) != len(bones):
        raise ValueError('Duplicate source bone names')
    aliases = ALIASES if name == 'Zhirelle' else {}
    for old, new in aliases.items():
        if old not in names or new in names:
            raise ValueError(f'Unexpected leg layout: {old}/{new}')
        bones[names[old]]['name'] = new
        bones.append(dict(name=old, parent=names[old], translation=[0, 0, 0],
                          rotation=[0, 0, 0, 1], scale=[1, 1, 1]))
    # Skin weights retain their original indices; added aliases carry no skin.
    for i, bone in enumerate(bones):
        if not -1 <= bone['parent'] < i or (i != 0 and bone['parent'] == -1):
            raise ValueError('Expected one root and parent-first bones')
        rotation = np.asarray(bone['rotation'], dtype=float)
        length = np.linalg.norm(rotation)
        if not np.isfinite(length) or abs(length - 1) > 1e-4:
            raise ValueError('Invalid source rotation')
        bone['rotation'] = (rotation / length).tolist()
    required = {'root', 'pelvis', 'head', 'hand_l', 'hand_r', 'thigh_l', 'thigh_r',
                'calf_l', 'calf_r', 'foot_l', 'foot_r', 'ball_l', 'ball_r'}
    if not required <= {b['name'] for b in bones}:
        raise ValueError('Missing player limb mapping')
    weights = vertices['weights']
    if np.any(weights['bone'] >= len(audit['bones'])) or np.any(weights['weight'] < 0):
        raise ValueError('Invalid source weights')
    sums = weights['weight'].sum(axis=1)
    if not np.isfinite(sums).all() or np.any(abs(sums - 1) > 1e-4):
        raise ValueError('Source weights are not normalized')
    rows, slots = np.nonzero(weights['weight'] > 0)
    influences = np.column_stack((rows, weights['bone'][rows, slots],
                                  weights['weight'][rows, slots] / sums[rows])).tolist()
    for row in influences:
        row[0], row[1] = int(row[0]), int(row[1])
    normals = channels['normal'][:, :3].astype(float)
    lengths = np.linalg.norm(normals, axis=1)
    if not np.isfinite(lengths).all() or np.any(lengths < .5):
        raise ValueError('Invalid source normals')
    normals /= lengths[:, None]
    uv = channels['uv'].reshape(count, -1)
    if not np.isfinite(uv).all() or not np.isfinite(vertices['position']).all():
        raise ValueError('Nonfinite surface data')
    wedges = [[i, *values] for i, values in enumerate(uv.tolist())]
    faces = []
    covered = np.zeros(len(indices), dtype=bool)
    for section in audit['sections']:
        begin = section['FirstIndex']; end = begin + 3 * section['NumFaces']
        if begin % 3 or end > len(indices) or covered[begin:end].any():
            raise ValueError('Invalid or overlapping section ranges')
        covered[begin:end] = True
        faces.extend([*tri, section['MaterialIndex']]
                     for tri in indices[begin:end].reshape(-1, 3).tolist())
    if not covered.all():
        raise ValueError('Unassigned triangles')
    prefix = f'/Game/CSS/eins0fx/NPC/{name}'
    packet = dict(schema=1, mesh_package=prefix + '/SK_Body',
                  skeleton_package=prefix + '/SKEL_Body', bones=bones,
                  materials=audit['material_slots'], points=vertices['position'].tolist(),
                  wedges=wedges, faces=faces, influences=influences,
                  normals=normals.tolist(), uv_channels=uv_count)
    if color_count:
        packet['colors'] = channels['color'][:, 0].tolist()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x') as stream:
        json.dump(packet, stream, separators=(',', ':'), allow_nan=False)
        stream.write('\n')
    recipe = dict(source=audit['asset'], source_hashes={r['name']: r['sha256'] for r in files},
                  source_audit_sha256=sha(source / 'skin-audit.json'),
                  output_sha256=sha(output), aliases=aliases, source_bones=len(audit['bones']),
                  candidate_bones=len(bones), vertices=count, triangles=len(faces),
                  geometry_changed=False, uv_changed=False, weights_remapped=False,
                  pending=['native animation compatibility', 'materials and original physics',
                           'additional LODs and cloth', 'cook and game acceptance'])
    output.with_suffix('.recipe.json').write_text(json.dumps(recipe, indent=2) + '\n')
    print(name, count, 'vertices,', len(bones), 'bones:', output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('name', choices=SOURCES)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    prepare(args.source, args.name, args.output)

"""Derive an Eve diagnostic rig with pelvis-parented legs and unchanged world poses."""
import argparse
import copy
import hashlib
import json
import re
from pathlib import Path

import numpy as np


def multiply(a, b):
    return np.concatenate((a[..., 3:] * b[..., :3] + b[..., 3:] * a[..., :3]
                           + np.cross(a[..., :3], b[..., :3]),
                           a[..., 3:] * b[..., 3:] - np.sum(a[..., :3] * b[..., :3], axis=-1, keepdims=True)), axis=-1)


def rotate(q, v):
    t = 2 * np.cross(q[..., :3], v)
    return v + q[..., 3:] * t + np.cross(q[..., :3], t)


def transforms(bones, tracks=None, frames=1):
    names = {bone['name'] for bone in bones}
    track_names = [track['name'] for track in (tracks or [])]
    if len(set(track_names)) != len(track_names) or not set(track_names) <= names:
        raise ValueError('Duplicate or unknown animation tracks')
    tracks = {track['name']: track['keys'] for track in (tracks or [])}
    result = []
    for index, bone in enumerate(bones):
        if not -1 <= bone['parent'] < index:
            raise ValueError('Expected parent-first skeleton')
        keys = tracks.get(bone['name'], [bone] * frames)
        if len(keys) != frames:
            raise ValueError('Track length does not match the sample clock')
        p, q, s = (np.array([key[field] for key in keys], dtype=float)
                   for field in ('translation', 'rotation', 'scale'))
        if p.shape != (frames, 3) or q.shape != (frames, 4) or s.shape != (frames, 3):
            raise ValueError('Invalid transform dimensions')
        if not all(np.isfinite(v).all() for v in (p, q, s)) or np.any(s <= 0):
            raise ValueError('Expected finite transforms with positive scale')
        lengths = np.linalg.norm(q, axis=-1, keepdims=True)
        if np.any(np.abs(lengths - 1) > 1e-4):
            raise ValueError('Expected normalized rotations')
        q = q / lengths
        if bone['parent'] >= 0:
            pp, pq, ps = result[bone['parent']]
            p, q, s = pp + rotate(pq, ps * p), multiply(pq, q), ps * s
        result.append((p, q, s))
    return result


def derive(bones, tracks=None, frames=1):
    lookup = {b['name']: i for i, b in enumerate(bones)}
    if len(lookup) != len(bones):
        raise ValueError('Duplicate bone names')
    parent = lookup['Bip001-Pelvis']
    old_parent = lookup['Bip001-Spine']
    thighs = [lookup[f'Bip001-{side}-Thigh'] for side in ('L', 'R')]
    if any(bones[i]['parent'] != old_parent for i in thighs):
        raise ValueError('Expected the audited spine-parented Eve thighs')
    world = transforms(bones, tracks, frames)
    pp, pq, ps = world[parent]
    inverse = pq.copy()
    inverse[..., :3] *= -1
    new_bones = copy.deepcopy(bones)
    new_tracks = copy.deepcopy(tracks or [])
    for index in thighs:
        p, q, s = world[index]
        lp, lq, ls = rotate(inverse, p - pp) / ps, multiply(inverse, q), s / ps
        lq /= np.linalg.norm(lq, axis=-1, keepdims=True)
        keys = [dict(translation=a.tolist(), rotation=b.tolist(), scale=c.tolist())
                for a, b, c in zip(lp, lq, ls, strict=True)]
        new_bones[index]['parent'] = parent
        if tracks is None:
            new_bones[index].update(keys[0])
        else:
            new_tracks = [t for t in new_tracks if t['name'] != bones[index]['name']]
            new_tracks.append(dict(name=bones[index]['name'], keys=keys))
    after = transforms(new_bones, new_tracks if tracks is not None else None, frames)
    error = [max(float(np.max(np.abs(a - b))) for old, new in zip(world, after, strict=True)
                 for a, b in [(old[channel], new[channel])]) for channel in range(3)]
    if max(error) > 1e-8:
        raise ValueError(f'Reparenting changed world transforms: {error}')
    return new_bones, new_tracks, error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--revision', required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9]{1,8}', args.revision) or args.output.exists():
        raise ValueError('Use a new output directory and short alphanumeric revision')
    inputs = {}

    def read(name):
        path = args.source / name
        inputs[str(path.resolve())] = hashlib.sha256(path.read_bytes()).hexdigest()
        return json.loads(path.read_text())

    mesh = read('source.mesh.json')
    original = copy.deepcopy(mesh['bones'])
    mesh['bones'], _, reference_error = derive(original)
    mesh['mesh_package'] = f'/Game/CSS/AnimLab/SK_{args.revision}_EveSource'
    mesh['skeleton_package'] = f'/Game/CSS/AnimLab/SKEL_{args.revision}_EveSource'
    batch = read('batch.json')
    config = read('batch-config.json')
    config['revision'] = args.revision
    config['leg_parent_normalization'] = 'World-preserving source thighs to Bip001-Pelvis'
    outputs = {'source.mesh.json': mesh, 'batch.json': batch, 'batch-config.json': config}
    errors = {}
    for _, filename in batch:
        if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,80}', filename):
            raise ValueError('Invalid clip filename')
        clip = read(filename + '.json')
        _, clip['tracks'], errors[filename] = derive(original, clip['tracks'], clip['frames'])
        outputs[filename + '.json'] = clip
    args.output.mkdir(parents=True)
    for filename, value in outputs.items():
        (args.output / filename).write_text(json.dumps(value, separators=(',', ':')) + '\n')
    if any(hashlib.sha256(Path(p).read_bytes()).hexdigest() != h for p, h in inputs.items()):
        raise ValueError('Input changed during derivation')
    (args.output / 'hierarchy-receipt.json').write_text(json.dumps(dict(
        inputs=inputs, reference_max_component_errors=reference_error,
        clip_max_component_errors=errors,
        scope='Derived source rig only. All sampled world transforms preserved. Retarget contact and visual acceptance remain required.'), indent=2) + '\n')


if __name__ == '__main__':
    main()

"""Retarget diagnostic ACL samples onto a cooked mesh before neutral skinning."""
import hashlib
import json

import numpy as np

from reparent_source_legs import multiply, rotate


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def retarget_local(values, track_names, bones, skeleton, reference):
    info = skeleton['ReferenceSkeleton']['FinalRefBoneInfo']
    names = [b['Name'] for b in info]
    mesh_names = [b['name'] for b in bones]
    modes = skeleton['Properties']['BoneTree']
    if (len(set(names)) != len(names) or len(set(mesh_names)) != len(mesh_names) or
            len(set(track_names)) != len(track_names) or len(modes) != len(names) or
            not set(mesh_names) <= set(names) or not set(mesh_names) <= set(track_names) or
            len(reference) < len(names) or values.shape[1:] != (len(track_names), 10)):
        raise ValueError('Incomplete or ambiguous skeleton/track coverage')
    result = np.array(values[:, [track_names.index(n) for n in mesh_names]], dtype=float)
    for index, bone in enumerate(bones):
        source = names.index(bone['name'])
        parent = info[source]['ParentIndex']
        if not -1 <= bone['parent'] < index or not -1 <= parent < source:
            raise ValueError('Skeleton is not parent-first')
        if (names[parent] if parent >= 0 else None) != (
                mesh_names[bone['parent']] if bone['parent'] >= 0 else None):
            raise ValueError('Mesh requires compatible-skeleton remapping')
        mode = modes[source]['TranslationRetargetingMode'].split('::')[-1]
        target = np.asarray(bone['translation'], dtype=float)
        # UE 5.6.1 AnimationRuntime.cpp, FAnimationRuntime::RetargetBoneTransform.
        if mode == 'Skeleton':
            result[:, index, 4:7] = target
        elif mode == 'AnimationScaled':
            original = reference[source]['Translation']
            length = np.linalg.norm([original[c] for c in 'XYZ'])
            if length > .0001:
                result[:, index, 4:7] *= np.linalg.norm(target) / length
        elif mode != 'Animation':
            raise ValueError('Unimplemented translation retarget mode: ' + mode)
    if (not np.isfinite(result).all() or np.any(result[:, :, 7:] <= 0) or
            np.max(np.abs(np.linalg.norm(result[:, :, :4], axis=2) - 1)) >= .001):
        raise ValueError('Invalid retargeted transform')
    return result


def world_transforms(local, bones):
    result = []
    for index, bone in enumerate(bones):
        if not -1 <= bone['parent'] < index:
            raise ValueError('Skeleton is not parent-first')
        q, p, s = local[:, index, :4], local[:, index, 4:7], local[:, index, 7:]
        q = q / np.linalg.norm(q, axis=1, keepdims=True)
        if bone['parent'] >= 0:
            pp, pq, ps = result[bone['parent']]
            p, q, s = pp + rotate(pq, ps * p), multiply(pq, q), ps * s
        result.append((p, q, s))
    return result


def load_cooked(path, bones, skeleton_path, inputs):
    sample_path = path.with_name('absolute-tracks.json')
    for p in (path, sample_path, skeleton_path):
        inputs[str(p.resolve())] = sha(p)
    manifest = json.loads(path.read_text())
    sample = json.loads(sample_path.read_text())
    exports = json.loads(skeleton_path.read_text())
    skeletons = [r for rows in exports.values() for r in rows if r.get('Type') == 'Skeleton']
    if len(skeletons) != 1:
        raise ValueError('Expected exactly one audited Skeleton export')
    if (manifest['schema'] != 1 or manifest['layout'] != 'frame,track,component' or
            manifest['componentType'] != 'float32-le' or manifest['file'] != 'absolute-tracks.bin' or
            manifest.get('roundingPolicy') != 'none' or
            manifest['components'] != ['qx', 'qy', 'qz', 'qw', 'tx', 'ty', 'tz', 'sx', 'sy', 'sz']):
        raise ValueError('Contact requires runtime-sampled absolute transforms')
    frames, names = manifest['frames'], manifest['names']
    substeps = manifest.get('substeps', 1)
    binary = path.with_name(manifest['file'])
    if (substeps not in (1, 2, 4) or not 1 <= frames <= 400001 or not 1 <= len(names) <= 4096 or
            frames * len(names) > 2004096 * substeps or binary.stat().st_size != frames * len(names) * 40 or
            binary.stat().st_size != manifest['bytes'] or sha(binary) != manifest['sha256'] or
            sample['schema'] != 2 or sample['asset'] != manifest['asset'] or
            sample['trackCount'] != len(names) or (substeps > 1 and sample['wrapOptimized']) or
            (sample['logicalSamples'] - 1) * substeps + 1 != frames or
            manifest['sampleRate'] != sample['sampleRate'] * substeps or
            not 0 < manifest['sampleRate'] <= 1000):
        raise ValueError('Invalid sample clock, dimensions or checksum')
    inputs[str(binary.resolve())] = manifest['sha256']
    values = np.memmap(binary, dtype='<f4', mode='r', shape=(frames, len(names), 10))
    if (not np.isfinite(values).all() or np.any(values[:, :, 7:] <= 0) or
            np.max(np.abs(np.linalg.norm(values[:, :, :4].astype(float), axis=2) - 1)) >= .001):
        raise ValueError('Invalid cooked transforms')
    for row in sample['frames']:
        expected = np.array([[t[field][c] for field, axes in (
            ('Rotation', 'XYZW'), ('Translation', 'XYZ'), ('Scale3D', 'XYZ')) for c in axes]
            for t in row['LocalTransforms']], dtype='<f4')
        if row['BoneNames'] != names or not np.array_equal(values[row['frame'] * substeps], expected):
            raise ValueError('Binary and JSON samples disagree')
    local = retarget_local(values, names, bones, skeletons[0], sample['referencePose'])
    return world_transforms(local, bones), dict(frames=frames, fps=manifest['sampleRate'],
        sampling='Cooked ACL with no rounding, mesh translation retargeting, neutral skinning',
        substeps=substeps, asset=manifest['asset'])

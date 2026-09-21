"""Fit source ankle paths and knee planes without changing the target proportions."""
import copy
import math

import numpy as np
from review_source_tracks import rotation


def matrix(value):
    scale = value.get('scale', [1., 1., 1.])
    # Ignore the fitted bind's floating-point round-off (up to 0.000027).
    # The original scale tracks remain untouched in the returned document.
    if not np.allclose(scale, 1., atol=1e-4, rtol=0):
        raise ValueError('Leg fitting requires near-unit local scales')
    result = np.eye(4)
    result[:3, :3] = rotation(value['rotation'])
    result[:3, 3] = value['translation']
    return result


def worlds(locals_, bind):
    result = []
    for index, (local, bone) in enumerate(zip(locals_, bind, strict=True)):
        parent = bone['parent']
        if not -1 <= parent < index:
            raise ValueError('Bind parents must precede their children')
        result.append(result[parent] @ local if parent >= 0 else local)
    return result


def swing(a, b):
    a = a / np.linalg.norm(a)
    b = b / np.linalg.norm(b)
    v = np.cross(a, b)
    c = float(np.dot(a, b))
    if c <= -.999:
        raise ValueError('Ambiguous opposite limb directions')
    k = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + k + k @ k / (1 + c)


def quaternion(m):
    k = np.array([
        [m[0,0]-m[1,1]-m[2,2], m[0,1]+m[1,0], m[0,2]+m[2,0], m[2,1]-m[1,2]],
        [m[0,1]+m[1,0], m[1,1]-m[0,0]-m[2,2], m[1,2]+m[2,1], m[0,2]-m[2,0]],
        [m[0,2]+m[2,0], m[1,2]+m[2,1], m[2,2]-m[0,0]-m[1,1], m[1,0]-m[0,1]],
        [m[2,1]-m[1,2], m[0,2]-m[2,0], m[1,0]-m[0,1], m.trace()]]) / 3
    q = np.linalg.eigh(k)[1][:, -1]
    if np.max(np.abs(rotation(q) - m)) >= 1e-6:
        raise ValueError('Fitted rotation is not orthonormal')
    return q


def fit(document, source, bind, source_bind):
    """Return a copy changing only thigh/calf/foot rotations on the two legs.

    Source translations are already in the target's facing basis. Ankle offsets
    from each hip scale by total leg length, with the original source knee plane.
    Root translation, pelvis motion, limb lengths and all other tracks stay intact.
    """
    result = copy.deepcopy(document)
    if len(result['frames']) != source['frames']:
        raise ValueError('Source and fitted clip frame counts differ')
    names = [b['name'] for b in bind]
    source_names = [b['name'] for b in source_bind]
    tracks = {t['name']: t['keys'] for t in source['tracks']}
    target_rest = worlds([matrix(b) for b in bind], bind)
    source_rest = worlds([matrix(b) for b in source_bind], source_bind)
    errors, old_x, new_x = [], [], []
    for frame, entry in enumerate(result['frames']):
        pose = entry['pose']['Snapshot']
        if pose['BoneNames'][:len(names)] != names:
            raise ValueError('Fitted snapshot bone order differs from bind')
        transforms = pose['LocalTransforms']
        locals_ = [matrix(dict(translation=[t['Translation'][k] for k in 'XYZ'],
                              rotation=[t['Rotation'][k] for k in 'XYZW'],
                              scale=[t['Scale3D'][k] for k in 'XYZ']))
                   for t in transforms[:len(bind)]]
        sw = worlds([matrix(tracks[b['name']][frame] if b['name'] in tracks else b)
                     for b in source_bind], source_bind)
        for side in ('l', 'r'):
            ids = [names.index(n+'_'+side) for n in ('thigh', 'calf', 'foot')]
            src = [source_names.index('Bip001-'+side.upper()+'-'+n)
                   for n in ('Thigh', 'Calf', 'Foot')]
            tw = worlds(locals_, bind)
            h, k, f = [tw[i][:3, 3].copy() for i in ids]
            sh, sk, sf = [sw[i][:3, 3] for i in src]
            a, b = np.linalg.norm(k-h), np.linalg.norm(f-k)
            sa, sb = np.linalg.norm(sk-sh), np.linalg.norm(sf-sk)
            if min(a, b, sa, sb) <= 1e-6:
                raise ValueError('Zero-length leg segment')
            goal = h + (sf-sh) * ((a+b)/(sa+sb))
            delta = goal-h
            distance = np.linalg.norm(delta)
            limit = a+b-1e-4
            if distance > limit:
                goal = h+delta*limit/distance
                distance = limit
            if distance <= abs(a-b)+1e-6:
                raise ValueError('Source ankle is inside the fitted leg reach limit')
            axis = (goal-h)/distance
            pole = sk-sh
            pole = pole-axis*np.dot(pole, axis)
            if np.linalg.norm(pole) <= 1e-4:
                raise ValueError('Source knee plane is undefined')
            pole /= np.linalg.norm(pole)
            along = (a*a-b*b+distance*distance)/(2*distance)
            knee = h+axis*along+pole*math.sqrt(max(0, a*a-along*along))
            desired = swing(k-h, knee-h) @ tw[ids[0]][:3, :3]
            locals_[ids[0]][:3, :3] = tw[bind[ids[0]]['parent']][:3, :3].T @ desired
            tw = worlds(locals_, bind)
            ck, cf = [tw[i][:3, 3] for i in ids[1:]]
            desired = swing(cf-ck, goal-ck) @ tw[ids[1]][:3, :3]
            locals_[ids[1]][:3, :3] = tw[ids[0]][:3, :3].T @ desired
            tw = worlds(locals_, bind)
            desired = sw[src[2]][:3, :3] @ source_rest[src[2]][:3, :3].T @ target_rest[ids[2]][:3, :3]
            locals_[ids[2]][:3, :3] = tw[ids[1]][:3, :3].T @ desired
            tw = worlds(locals_, bind)
            error = float(np.linalg.norm(tw[ids[2]][:3, 3]-goal))
            if error >= .001:
                raise ValueError('Fitted ankle missed the source trajectory')
            errors.append(error)
            old_x.append(float(f[0]))
            new_x.append(float(goal[0]))
            for i in ids:
                transforms[i]['Rotation'] = dict(zip('XYZW', quaternion(locals_[i][:3, :3]).tolist()))
    return result, dict(frames=len(result['frames']), max_ankle_error_cm=max(errors),
                       old_lateral_span_cm=max(old_x)-min(old_x),
                       new_lateral_span_cm=max(new_x)-min(new_x))

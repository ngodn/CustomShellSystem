"""Geometry helpers for isolated B2 collision diagnostics in Blender 5.2.2."""
import math

import numpy as np
from mathutils import Quaternion

SKIN_SLOTS = set(range(8)) | {23, 24, 25, 26}
PUBLIC_MORPHS = ('FBMBodyTone', 'PBMBreastsSize', 'PBMGlutesSize',
                 'PBMHipSize', 'PBMThighsTone', 'PBMWaistWidth')


def rotator(value):
    # Pinned UE 5.6.1 FRotator3d::Quaternion.
    p, y, r = [math.radians(value.get(k, 0))/2 for k in ('Pitch', 'Yaw', 'Roll')]
    sp, sy, sr = math.sin(p), math.sin(y), math.sin(r)
    cp, cy, cr = math.cos(p), math.cos(y), math.cos(r)
    return Quaternion((cr*cp*cy+sr*sp*sy, cr*sp*sy-sr*cp*cy,
                       -cr*sp*cy-sr*cp*sy, cr*cp*sy-sr*sp*cy)).normalized()


def to_rotator(q):
    # Pinned UE 5.6.1 FQuat4d::Rotator, including its singularity branches.
    w, x, y, z = q.normalized()
    singularity = z*x-w*y
    if singularity < -.4999995:
        p, yaw, r = -90., math.degrees(-2*math.atan2(x, w)), 0.
    elif singularity > .4999995:
        p, yaw, r = 90., math.degrees(2*math.atan2(x, w)), 0.
    else:
        p = math.degrees(math.asin(2*singularity))
        yaw = math.degrees(math.atan2(2*(w*z+x*y), 1-2*(y*y+z*z)))
        r = math.degrees(math.atan2(-2*(w*x+y*z), 1-2*(x*x+y*y)))
    return dict(Pitch=p, Yaw=(yaw+180)%360-180, Roll=r)


def xyz(value):
    return np.asarray([value.get(k, 0.) for k in 'XYZ'])


def vector(value):
    return dict(zip('XYZ', map(float, value)))


def sdf(shape, kind, cloud):
    rotation = np.asarray(rotator(shape['Rotation']).to_matrix(), dtype=float)
    points = (cloud-xyz(shape['Center'])) @ rotation
    if kind == 'BoxElems':
        q = np.abs(points)-np.asarray([shape[k] for k in 'XYZ'])/2
        return np.linalg.norm(np.maximum(q, 0), axis=1)+np.minimum(np.max(q, axis=1), 0)
    assert kind in ('SphylElems', 'TaperedCapsuleElems'), kind
    radius = shape.get('Radius', shape.get('Radius0'))
    assert radius == shape.get('Radius1', radius), 'Unequal tapered radii need a different SDF'
    q = np.column_stack((points[:, :2], np.maximum(np.abs(points[:, 2])-shape['Length']/2, 0)))
    return np.linalg.norm(q, axis=1)-radius


def skin_regions(source, bones, bodies):
    core = {b['BoneName'].lower() for b in bodies}
    triangles = [[source['wedges'][w][0] for w in f[:3]]
                 for f in source['faces'] if f[3] in SKIN_SLOTS]
    ids = sorted({i for f in triangles for i in f})
    assert len(ids) == 24480
    ancestors = []
    for b in bones:
        name = b['name'].lower()
        ancestors.append(name if name in core else ancestors[b['parent']] if b['parent'] >= 0 else None)
    weights = {i: {} for i in ids}
    for vertex, bone, weight in source['influences']:
        if vertex in weights and ancestors[bone]:
            row = weights[vertex]
            name = ancestors[bone]
            row[name] = row.get(name, 0.)+weight
    regions = {name: [] for name in sorted(core)}
    for vertex, row in weights.items():
        assert row
        regions[max(row, key=row.get)].append(vertex)
    return ids, regions


def volume(shape, kind):
    if kind == 'BoxElems':
        return math.prod(shape[k] for k in 'XYZ')
    radius = shape.get('Radius', shape.get('Radius0'))
    assert radius == shape.get('Radius1', radius)
    return math.pi*radius*radius*shape['Length']+4/3*math.pi*radius**3

"""Original-Eve distal hinges and experimental proximal swing adaptation.

Consumes calibrated local rotations. It preserves independent curls, thumb-base
motion and metacarpals. The gain is a study parameter, not an anatomical limit.
"""
import copy
import math
from mathutils import Quaternion


def parameters(curves):
    unique = {m['bone']: m for m in curves}
    assert len(unique) == 15
    return [dict(bone=m['bone'], index=m['index'], left_wxyz=m['left_wxyz'],
                 right_wxyz=m['right_wxyz'], hinge=not m['bone'].endswith('_01_l'))
            for m in unique.values() if m['bone'] != 'thumb_01_l']


def apply(doc, models, gain=.125):
    assert 0 <= gain <= 1
    result = copy.deepcopy(doc)
    output = {}
    minimum = 1.
    for m in models:
        t = doc['pose']['Snapshot']['LocalTransforms'][m['index']]
        q = Quaternion([t['Rotation'][a] for a in 'WXYZ']).normalized()
        left, right = Quaternion(m['left_wxyz']), Quaternion(m['right_wxyz'])
        d = (left @ q @ right).normalized()
        if d.w < 0:
            d.negate()
        norm = math.hypot(d.w, d.x)
        minimum = min(minimum, norm)
        if norm <= 1e-4:
            return result, dict(valid=False, minimum_projection_norm=minimum)
        twist = Quaternion((d.w/norm, d.x/norm, 0, 0))
        swing = d @ twist.inverted()
        desired = twist if m['hinge'] else Quaternion((1, 0, 0, 0)).slerp(swing, gain) @ twist
        output[m['index']] = (left.inverted() @ desired @ right.inverted()).normalized()
    for i, q in output.items():
        result['pose']['Snapshot']['LocalTransforms'][i]['Rotation'] = dict(zip('XYZW', (q.x, q.y, q.z, q.w)))
    return result, dict(valid=True, minimum_projection_norm=minimum)

"""Experimental continuous source-space thumb-web guard, before correctives.

This changes only source Z at the thumb base. Parameters belong to V44B2;
they are not universal joint limits and do not replace fingertip clearance.
"""
import copy
import math
from mathutils import Quaternion


def apply(doc, model, driver):
    assert model['bone'] == driver['bone']
    assert model['source_euler_order'] == driver['euler_order']
    assert doc['pose']['Snapshot']['BoneNames'][driver['index']].lower() == model['bone']
    candidate = copy.deepcopy(doc)
    transform = candidate['pose']['Snapshot']['LocalTransforms'][driver['index']]
    incoming = Quaternion([transform['Rotation'][k] for k in 'WXYZ']).normalized()
    source = (Quaternion(driver['left_wxyz']) @ incoming @ Quaternion(driver['right_wxyz'])).normalized()
    angles = source.to_euler(model['source_euler_order'])
    x, y, z = [math.degrees(v) for v in angles]
    lower = min(model['lower_z_ceiling'], model['lower_z_x_coefficient'] * x +
                model['lower_z_y_coefficient'] * y + model['lower_z_intercept'])
    requested = max(0.0, lower - z)
    correction = min(model['maximum_correction'], requested)
    if correction > model['unchanged_epsilon']:
        angles.z += math.radians(correction)
        q = (Quaternion(driver['left_wxyz']).inverted() @ angles.to_quaternion() @ Quaternion(driver['right_wxyz']).inverted()).normalized()
        transform['Rotation'] = dict(zip('XYZW', (q.x, q.y, q.z, q.w)))
    else:
        correction = 0.0
    return candidate, dict(correction_degrees=correction, source_euler_degrees=[x,y,z],
                          lower_z_degrees=lower, correction_saturated=requested > model['maximum_correction'])

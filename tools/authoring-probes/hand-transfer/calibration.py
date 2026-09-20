"""Offline reference for the existing two-anchor local hand calibration."""
import copy
import math
from mathutils import Quaternion, Vector


def apply(doc, models, compatible=False):
    result = copy.deepcopy(doc)
    output = {}
    for m in models:
        incoming = doc['pose']['Snapshot']['LocalTransforms'][m['index']]['Rotation']
        raw = Quaternion([incoming[k] for k in 'WXYZ']).normalized()
        if compatible:
            raw = (Quaternion(m['compatible_q0']).inverted() @ raw @ Quaternion(m['compatible_q1']).inverted()).normalized()
        delta = (Quaternion(m['source_ref']).inverted() @ raw).normalized()
        if delta.w < 0:delta.negate()
        if m['axis'] is not None:
            axis = Vector(m['axis'])
            projection = Vector((delta.x,delta.y,delta.z)).dot(axis)
            norm = math.hypot(delta.w,projection)
            if norm <= 1e-5:return result,dict(valid=False)
            twist = Quaternion((delta.w/norm,*(axis*(projection/norm))))
            delta = delta @ twist.inverted() @ Quaternion(axis,2*math.atan2(projection,delta.w)*m['ratio'])
        transport = Quaternion(m['transport'])
        output[m['index']] = (Quaternion(m['target_ref']) @ transport @ delta @ transport.inverted()).normalized()
    for i,q in output.items():
        result['pose']['Snapshot']['LocalTransforms'][i]['Rotation'] = dict(zip('XYZW',(q.x,q.y,q.z,q.w)))
    return result,dict(valid=True)

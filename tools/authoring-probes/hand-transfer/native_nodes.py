"""Shipped RigVM nodes for the experimental per-mesh hand adapter."""
import math

from build_controlrig_chain_probe import Pin, TYPES


def quaternion(wxyz, inverse=False):
    w, x, y, z = wxyz
    if inverse:
        x, y, z = -x, -y, -z
    return f'(X={x:.12g},Y={y:.12g},Z={z:.12g},W={w:.12g})'


def append_calibration(g, start, models):
    """Snapshot inputs, calculate all joints, then apply only if nonsingular."""
    TYPES['FQuat'] = '/Script/CoreUObject.Quat'
    g.member('Inputs', 'TArray<FTransform>', '(' + ','.join('()' for _ in models) + ')')
    g.member('Outputs', 'TArray<FQuat>', '(' + ','.join('(X=0,Y=0,Z=0,W=1)' for _ in models) + ')')
    g.member('ValidInput', 'bool', 'False', True)
    g.member('InputIsV43Compatible', 'bool', 'False', True)
    start = g.set(start, 'ValidInput', True)
    for i, m in enumerate(models):
        incoming = g.unit('RigUnit_GetTransform', Space='LocalSpace', bInitial=False)
        g.value(incoming + '.Item.Type', 'Bone')
        g.value(incoming + '.Item.Name', m['bone'])
        start = g.put(start, 'Inputs', i, Pin(incoming + '.Transform'))
    for i, m in enumerate(models):
        incoming = g.at('Inputs', i, 'Rotation')
        recovered = g.math('QuaternionMul', A=quaternion(m['compatible_q0'], True), B=incoming)
        recovered = g.math('QuaternionMul', A=recovered, B=quaternion(m['compatible_q1'], True))
        raw = g.math('QuaternionSelectBool', Condition=g.get('InputIsV43Compatible'), IfTrue=recovered, IfFalse=incoming)
        delta = g.math('QuaternionUnit', Value=g.math('QuaternionMul', A=quaternion(m['source_ref'], True), B=raw))
        if m['axis'] is not None:
            # Quaternions are atomic pins in RigVM. Dot products expose the
            # needed components without relying on nonexistent XYZ/W subpins.
            projection = g.math('QuaternionDot', A=delta, B=quaternion([0, *m['axis']]))
            w = g.math('QuaternionDot', A=delta, B=quaternion([1, 0, 0, 0]))
            norm_squared = g.math('FloatAdd', A=g.math('FloatMul', A=projection, B=projection), B=g.math('FloatMul', A=w, B=w))
            valid = g.math('FloatGreater', A=norm_squared, B=1e-10)
            start = g.set(start, 'ValidInput', g.math('BoolAnd', A=g.get('ValidInput'), B=valid))
            # Swing is unchanged by quaternion antipodes. Canonicalize the
            # two angle inputs only, retaining a shortest signed twist.
            sign = g.math('FloatSelectBool', Condition=g.math('FloatLess', A=w, B=0), IfTrue=-1, IfFalse=1)
            angle = g.math('FloatMul', A=g.math('FloatAtan2', A=g.math('FloatMul', A=projection, B=sign), B=g.math('FloatAbs', Value=w)), B=2 * m['ratio'])
            split = g.unit('RigVMFunction_MathQuaternionSwingTwist', Input=delta, TwistAxis=m['axis'])
            twist = g.math('QuaternionFromAxisAndAngle', Axis=m['axis'], Angle=angle)
            delta = g.math('QuaternionMul', A=Pin(split + '.Swing'), B=twist)
        transported = g.math('QuaternionMul', A=quaternion(m['transport']), B=delta)
        transported = g.math('QuaternionMul', A=transported, B=quaternion(m['transport'], True))
        output = g.math('QuaternionUnit', Value=g.math('QuaternionMul', A=quaternion(m['target_ref']), B=transported))
        start = g.put(start, 'Outputs', i, output)
    active = g.math('BoolAnd', A=g.get('Enabled'), B=g.get('ValidInput'))
    start, bypass = g.branch(start, active)
    for i, m in enumerate(models):
        node = g.unit('RigUnit_SetTransform', Space='LocalSpace', bInitial=False, bPropagateToChildren=True)
        g.link(start, node + '.ExecuteContext')
        g.value(node + '.Item.Type', 'Bone')
        g.value(node + '.Item.Name', m['bone'])
        g.value(node + '.Value.Translation', g.at('Inputs', i, 'Translation'))
        g.value(node + '.Value.Scale3D', g.at('Inputs', i, 'Scale3D'))
        g.value(node + '.Value.Rotation', g.at('Outputs', i))
        start = Pin(node + '.ExecuteContext')
    return start, bypass


def append_correctives(g, controller, start, models, enabled):
    for m in models:
        incoming = g.unit('RigUnit_GetTransform', Space='LocalSpace', bInitial=False)
        g.value(incoming + '.Item.Type', 'Bone')
        g.value(incoming + '.Item.Name', m['bone'])
        q = g.math('QuaternionMul', A=quaternion(m['left_wxyz']), B=Pin(incoming + '.Transform.Rotation'))
        q = g.math('QuaternionUnit', Value=g.math('QuaternionMul', A=q, B=quaternion(m['right_wxyz'])))
        euler = g.unit('RigVMFunction_MathQuaternionToEuler', Value=q, RotationOrder=m['euler_order'])
        radians = g.math('FloatMul', A=Pin(euler + '.Result.' + m['axis']), B=math.pi / 180)
        desired = g.math('FloatClamp', Value=g.math('FloatMul', A=radians, B=m['coefficient']), Minimum=0, Maximum=1)
        weight = g.math('FloatSub', A=desired, B=m['baked_value'])
        weight = g.math('FloatSelectBool', Condition=enabled, IfTrue=weight, IfFalse=0)
        node = g.unit('RigUnit_SetCurveValue', Curve=m['shape'], Value=weight)
        g.link(start, node + '.ExecuteContext')
        start = Pin(node + '.ExecuteContext')
    return start


def clear_correctives(g, start, models):
    for m in models:
        node = g.unit('RigUnit_SetCurveValue', Curve=m['shape'], Value=0)
        g.link(start, node + '.ExecuteContext')
        start = Pin(node + '.ExecuteContext')
    return start


def append_web_guard(g, start, model, driver):
    """Guard already calibrated/cleared input, before evaluating correctives."""
    assert model['bone'] == driver['bone']
    assert model['source_euler_order'] == driver['euler_order']
    g.member('WebInput', 'FTransform', '')
    g.member('WebCorrectionDegrees', 'float', '0', True)
    incoming = g.unit('RigUnit_GetTransform', Space='LocalSpace', bInitial=False)
    g.value(incoming + '.Item.Type', 'Bone')
    g.value(incoming + '.Item.Name', model['bone'])
    start = g.set(start, 'WebInput', Pin(incoming + '.Transform'))
    rotation = g.get('WebInput', 'Rotation')
    source = g.math('QuaternionMul', A=quaternion(driver['left_wxyz']), B=rotation)
    source = g.math('QuaternionUnit', Value=g.math('QuaternionMul', A=source, B=quaternion(driver['right_wxyz'])))
    euler = g.unit('RigVMFunction_MathQuaternionToEuler', Value=source, RotationOrder=model['source_euler_order'])
    x, y, z = [Pin(euler + '.Result.' + axis) for axis in 'XYZ']
    lower = g.math('FloatAdd', A=g.math('FloatMul', A=x, B=model['lower_z_x_coefficient']),
                   B=g.math('FloatMul', A=y, B=model['lower_z_y_coefficient']))
    lower = g.math('FloatMin', A=g.math('FloatAdd', A=lower, B=model['lower_z_intercept']), B=model['lower_z_ceiling'])
    correction = g.math('FloatClamp', Value=g.math('FloatSub', A=lower, B=z), Minimum=0, Maximum=model['maximum_correction'])
    active = g.math('FloatGreater', A=correction, B=model['unchanged_epsilon'])
    correction = g.math('FloatSelectBool', Condition=active, IfTrue=correction, IfFalse=0)
    start = g.set(start, 'WebCorrectionDegrees', correction)
    output = g.unit('RigVMFunction_MathQuaternionFromEuler', RotationOrder=model['source_euler_order'])
    g.value(output + '.Euler.X', x)
    g.value(output + '.Euler.Y', y)
    g.value(output + '.Euler.Z', g.math('FloatAdd', A=z, B=g.get('WebCorrectionDegrees')))
    mapped = g.math('QuaternionMul', A=quaternion(driver['left_wxyz'], True), B=Pin(output + '.Result'))
    mapped = g.math('QuaternionUnit', Value=g.math('QuaternionMul', A=mapped, B=quaternion(driver['right_wxyz'], True)))
    selected = g.math('QuaternionSelectBool', Condition=active, IfTrue=mapped, IfFalse=rotation)
    node = g.unit('RigUnit_SetTransform', Space='LocalSpace', bInitial=False, bPropagateToChildren=True)
    g.link(start, node + '.ExecuteContext')
    g.value(node + '.Item.Type', 'Bone')
    g.value(node + '.Item.Name', model['bone'])
    g.value(node + '.Value.Translation', g.get('WebInput', 'Translation'))
    g.value(node + '.Value.Scale3D', g.get('WebInput', 'Scale3D'))
    g.value(node + '.Value.Rotation', selected)
    return Pin(node + '.ExecuteContext')

"""Bounded directional finger clearance built from shipped RigVM nodes.

Authoring code only. Runtime arithmetic and loops execute in the native VM.
Inputs must already contain calibration and original-source articulation.
"""
import math
from build_controlrig_chain_probe import Pin


def loop(g, start, count):
    node = g.unit('RigVMFunction_ForLoopCount', Count=count)
    g.link(start, node + '.ExecuteContext')
    return Pin(node + '.Index'), Pin(node + '.ExecuteContext'), Pin(node + '.Completed')


def scalar(g, op, a, b):
    return g.math('Float' + op, A=a, B=b)


def select(g, condition, a, b, kind='Float'):
    if kind == 'Int':
        flag = g.math('BoolToInteger', Value=condition)
        return g.math('IntAdd', A=b, B=g.math('IntMul', A=flag, B=g.math('IntSub', A=a, B=b)))
    return g.math(kind + 'SelectBool', Condition=condition, IfTrue=a, IfFalse=b)


def array(g, name, kind, values):
    if kind == 'FVector':
        values = ['(' + ','.join(f'{axis}={v:.12g}' for axis, v in zip('XYZ', p)) + ')' for p in values]
    g.member(name, 'TArray<' + kind + '>', '(' + ','.join(map(str, values)) + ')')


def closest(g, start, a, b, c, d):
    """Finite-segment distance, including all endpoint and interior candidates."""
    u, v, w = g.sub(b, a), g.sub(d, c), g.sub(a, c)
    dot = lambda x, y: g.math('VectorDot', A=x, B=y)
    aa, bb, cc, dd, ee = dot(u, u), dot(u, v), dot(v, v), dot(u, w), dot(v, w)
    denom = scalar(g, 'Sub', scalar(g, 'Mul', aa, cc), scalar(g, 'Mul', bb, bb))
    safe = scalar(g, 'Max', denom, 1e-12)
    s = scalar(g, 'Div', scalar(g, 'Sub', scalar(g, 'Mul', bb, ee), scalar(g, 'Mul', cc, dd)), safe)
    t = scalar(g, 'Div', scalar(g, 'Sub', scalar(g, 'Mul', aa, ee), scalar(g, 'Mul', bb, dd)), safe)
    valid = scalar(g, 'Greater', denom, 1e-12)
    for value in (s, t):
        valid = g.math('BoolAnd', A=valid, B=g.math('BoolAnd',
            A=scalar(g, 'GreaterEqual', value, 0), B=scalar(g, 'LessEqual', value, 1)))
    best_squared = 1e30
    best_s = best_t = 0
    # Invalid interior candidates lose to every finite endpoint candidate.
    candidates = [(s, t, valid)]
    for endpoint in (0., 1.):
        proj = scalar(g, 'Div', scalar(g, 'Add', ee, scalar(g, 'Mul', endpoint, bb)), scalar(g, 'Max', cc, 1e-12))
        candidates.append((endpoint, g.math('FloatClamp', Value=proj, Minimum=0, Maximum=1), True))
    for endpoint in (0., 1.):
        proj = scalar(g, 'Div', scalar(g, 'Sub', scalar(g, 'Mul', endpoint, bb), dd), scalar(g, 'Max', aa, 1e-12))
        candidates.append((g.math('FloatClamp', Value=proj, Minimum=0, Maximum=1), endpoint, True))
    for s, t, active in candidates:
        # Compare squared distances using relative segment vectors. Build
        # the winning world-space points once, after selecting parameters.
        residual = g.sub(g.add(w, g.scale(u, s)), g.scale(v, t))
        squared = dot(residual, residual)
        take = g.math('BoolAnd', A=active, B=scalar(g, 'Less', squared, best_squared))
        best_s = select(g, take, s, best_s)
        best_t = select(g, take, t, best_t)
        best_squared = select(g, take, squared, best_squared)
    start = g.set(start, 'ClosestA', g.add(a, g.scale(u, best_s)))
    start = g.set(start, 'ClosestB', g.add(c, g.scale(v, best_t)))
    start = g.set(start, 'ClosestDistance', g.length(g.sub(g.get('ClosestB'), g.get('ClosestA'))))
    return start


def geometry(g, start, models):
    for fi, m in enumerate(models):
        parent = g.at('ClearanceParents', fi)
        offset = g.math('QuaternionFromAxisAndAngle', Axis=g.at('ClearanceAxes', fi), Angle=g.at('MeasureAngles', fi))
        rotation = g.math('QuaternionMul', A=offset, B=g.at('ClearanceLocals', fi*3, 'Rotation'))
        local = g.math('TransformMake', Translation=g.at('ClearanceLocals', fi*3, 'Translation'),
                       Rotation=rotation, Scale=g.at('ClearanceLocals', fi*3, 'Scale3D'))
        for segment in range(3):
            i = fi*3+segment
            if segment:
                parent = g.at('SegmentTransforms', i-1)
                local = g.at('ClearanceLocals', i)
            world = g.math('TransformMul', A=local, B=parent)
            start = g.put(start, 'SegmentTransforms', i, world)
            start = g.put(start, 'SegmentStarts', i, g.at('SegmentTransforms', i, 'Translation'))
            if segment:
                start = g.put(start, 'SegmentEnds', i-1, g.at('SegmentTransforms', i, 'Translation'))
        tip = g.math('TransformTransformVector', Transform=g.at('SegmentTransforms', fi*3+2), Location=m['tip_local'])
        start = g.put(start, 'SegmentEnds', fi*3+2, tip)
    # Every radial is posed once per candidate angle vector. Contact pairs
    # then reuse these values instead of rotating the same sample repeatedly.
    i, body, done = loop(g, start, g.get('HullPointCount'))
    segment = g.at('HullSegments', i)
    radial = g.math('VectorMul', A=g.at('HullPoints', i), B=g.at('SegmentTransforms', segment, 'Scale3D'))
    radial = g.math('QuaternionRotateVector', Transform=g.at('SegmentTransforms', segment, 'Rotation'), Vector=radial)
    g.put(body, 'PosedRadials', i, radial)
    return done


def pair_gap(g, start, pair, baseline):
    f, h = g.at('PairSegmentA', pair), g.at('PairSegmentB', pair)
    start = closest(g, start, g.at('SegmentStarts', f), g.at('SegmentEnds', f),
                    g.at('SegmentStarts', h), g.at('SegmentEnds', h))
    valid = scalar(g, 'Greater', g.get('ClosestDistance'), 1e-6)
    start = g.set(start, 'ClearanceValid', g.math('BoolAnd', A=g.get('ClearanceValid'), B=valid))
    normal = g.scale(g.sub(g.get('ClosestB'), g.get('ClosestA')),
                     scalar(g, 'Div', 1, scalar(g, 'Max', g.get('ClosestDistance'), 1e-6)))
    start = g.set(start, 'SeparationNormal', normal)
    # A capsule radius bounds every directional hull support. Skip its
    # expensive support loops when even that lower gap cannot beat the
    # current violating pair. Derivative samples always evaluate fully.
    lower = scalar(g, 'Sub', scalar(g, 'Sub', g.get('ClosestDistance'), g.at('HullRadiusBounds', f)), g.at('HullRadiusBounds', h))
    _, body, completed = loop(g, start, 1)
    skip = g.math('BoolAnd', A=baseline, B=scalar(g, 'GreaterEqual', lower, g.get('WorstGap')))
    skipped, start = g.branch(body, skip)
    g.set(skipped, 'MeasureGap', lower)
    for field, segment, sign in (('SupportA', f, 1), ('SupportB', h, -1)):
        start = g.set(start, field, 0)
        i, body, end = loop(g, start, g.at('HullCounts', segment))
        index = g.math('IntAdd', A=g.at('HullStarts', segment), B=i)
        radial = g.at('PosedRadials', index)
        support = g.math('VectorDot', A=radial, B=g.scale(g.get('SeparationNormal'), sign))
        g.set(body, field, scalar(g, 'Max', support, g.get(field)))
        start = end
    g.set(start, 'MeasureGap', scalar(g, 'Sub', scalar(g, 'Sub', g.get('ClosestDistance'), g.get('SupportA')), g.get('SupportB')))
    return completed


def append_finger_clearance(g, start, model):
    """Four proximal splay angles, bounded iterations, no hierarchy writes yet."""
    models = model['models'][1:]
    assert len(models) == 4
    bones = model['bones']
    pairs = [(f, f+1, s, t) for f in range(3) for s in range(3) for t in range(3)]
    for name in ('ClearanceAngles', 'MeasureAngles', 'ClearanceGradient'):
        array(g, name, 'float', [0]*4)
    for name, count in (('ClearanceParents', 4), ('ClearanceLocals', 12), ('SegmentTransforms', 12)):
        array(g, name, 'FTransform', ['()']*count)
    for name, count in (('ClearanceAxes', 4), ('SegmentStarts', 12), ('SegmentEnds', 12)):
        array(g, name, 'FVector', [[0, 0, 0]]*count)
    points, starts, counts = [], [], []
    for m in models:
        for hull in m['hull_radials_local']:
            starts.append(len(points)); counts.append(len(hull)); points.extend(hull)
    array(g, 'HullPoints', 'FVector', points)
    array(g, 'PosedRadials', 'FVector', [[0, 0, 0]]*len(points))
    array(g, 'HullSegments', 'int32', [i for i,n in enumerate(counts) for _ in range(n)])
    g.member('HullPointCount', 'int32', str(len(points)))
    array(g, 'HullStarts', 'int32', starts)
    array(g, 'HullCounts', 'int32', counts)
    array(g, 'HullRadiusBounds', 'float', [max(math.sqrt(sum(x*x for x in p)) for p in hull) for m in models for hull in m['hull_radials_local']])
    array(g, 'PairSegmentA', 'int32', [f*3+s for f, h, s, t in pairs])
    array(g, 'PairSegmentB', 'int32', [h*3+t for f, h, s, t in pairs])
    array(g, 'PairFingerA', 'int32', [p[0] for p in pairs])
    array(g, 'PairFingerB', 'int32', [p[1] for p in pairs])
    for name in ('ClosestA', 'ClosestB', 'SeparationNormal'):
        g.member(name, 'FVector', '(X=0,Y=0,Z=0)')
    for name in ('ClosestDistance', 'SupportA', 'SupportB', 'MeasureGap', 'WorstGap', 'PositiveGap', 'GradientNorm', 'StepFactor'):
        g.member(name, 'float', '0', True)
    g.member('ClosestTake', 'bool', 'False')
    g.member('ClearanceValid', 'bool', 'True', True)
    g.member('ClearanceRunning', 'bool', 'True')
    g.member('WorstPair', 'int32', '0', True)
    g.member('ClearanceIterations', 'int32', '0', True)
    start = g.set(start, 'ClearanceValid', True)
    start = g.set(start, 'ClearanceRunning', True)
    start = g.set(start, 'ClearanceIterations', 0)
    hand = g.unit('RigUnit_GetTransform', Space='GlobalSpace', bInitial=False)
    g.value(hand+'.Item.Type', 'Bone'); g.value(hand+'.Item.Name', 'hand_l')
    normal = g.math('QuaternionRotateVector', Transform=Pin(hand+'.Transform.Rotation'), Vector=model['palm_normal_local'])
    def require_unit_scale(execution, value):
        difference = g.sub(value, [1, 1, 1])
        squared = g.math('VectorDot', A=difference, B=difference)
        valid = scalar(g, 'LessEqual', squared, 1e-10)
        return g.set(execution, 'ClearanceValid', g.math('BoolAnd', A=g.get('ClearanceValid'), B=valid))
    for fi, m in enumerate(models):
        parent_name = bones[bones[m['indices'][0]]['parent']]['name']
        parent = g.unit('RigUnit_GetTransform', Space='GlobalSpace', bInitial=False)
        g.value(parent+'.Item.Type', 'Bone'); g.value(parent+'.Item.Name', parent_name)
        start = g.put(start, 'ClearanceParents', fi, Pin(parent+'.Transform'))
        start = require_unit_scale(start, g.at('ClearanceParents', fi, 'Scale3D'))
        axis = g.math('QuaternionRotateVector', Transform=g.math('QuaternionInverse', Value=Pin(parent+'.Transform.Rotation')), Vector=normal)
        start = g.put(start, 'ClearanceAxes', fi, axis)
        start = g.put(start, 'ClearanceAngles', fi, 0)
        for segment, bi in enumerate(m['indices']):
            node = g.unit('RigUnit_GetTransform', Space='LocalSpace', bInitial=False)
            g.value(node+'.Item.Type', 'Bone'); g.value(node+'.Item.Name', bones[bi]['name'])
            start = g.put(start, 'ClearanceLocals', fi*3+segment, Pin(node+'.Transform'))
            start = require_unit_scale(start, g.at('ClearanceLocals', fi*3+segment, 'Scale3D'))
    iteration, body, completed = loop(g, start, model['iteration_limit'])
    active, _ = g.branch(body, g.math('BoolAnd', A=g.get('ClearanceRunning'), B=g.get('ClearanceValid')))
    for fi in range(4):
        active = g.put(active, 'ClearanceGradient', fi, 0)
    measure, body, measured = loop(g, active, 5)
    active, _ = g.branch(body, g.get('ClearanceRunning'))
    baseline = g.math('IntEquals', A=measure, B=0)
    second = g.math('IntGreaterEqual', A=measure, B=3)
    finger = select(g, second, g.at('PairFingerB', g.get('WorstPair')), g.at('PairFingerA', g.get('WorstPair')), 'Int')
    plus = g.math('BoolOr', A=g.math('IntEquals', A=measure, B=1), B=g.math('IntEquals', A=measure, B=3))
    epsilon = math.radians(model['gradient_step_degrees'])
    for fi in range(4):
        perturb = g.math('BoolAnd', A=g.math('BoolNot', Value=baseline), B=g.math('IntEquals', A=finger, B=fi))
        offset = select(g, perturb, select(g, plus, epsilon, -epsilon), 0)
        active = g.put(active, 'MeasureAngles', fi, scalar(g, 'Add', g.at('ClearanceAngles', fi), offset))
    active = geometry(g, active, models)
    active = g.set(active, 'WorstGap', select(g, baseline, model['margin_cm']-.0001, g.get('WorstGap')))
    pair_index, body, evaluated = loop(g, active, select(g, baseline, len(pairs), 1, 'Int'))
    pair = select(g, baseline, pair_index, g.get('WorstPair'), 'Int')
    body = pair_gap(g, body, pair, baseline)
    take = g.math('BoolAnd', A=baseline, B=scalar(g, 'Less', g.get('MeasureGap'), g.get('WorstGap')))
    body = g.set(body, 'ClosestTake', take)
    body = g.set(body, 'WorstPair', select(g, g.get('ClosestTake'), pair_index, g.get('WorstPair'), 'Int'))
    g.set(body, 'WorstGap', select(g, g.get('ClosestTake'), g.get('MeasureGap'), g.get('WorstGap')))
    baseline_body, derivative = g.branch(evaluated, baseline)
    g.set(baseline_body, 'ClearanceRunning', scalar(g, 'Less', g.get('WorstGap'), model['margin_cm']-.0001))
    positive, negative = g.branch(derivative, plus)
    g.set(positive, 'PositiveGap', g.get('MeasureGap'))
    g.put(negative, 'ClearanceGradient', finger, scalar(g, 'Div', scalar(g, 'Sub', g.get('PositiveGap'), g.get('MeasureGap')), 2*epsilon))
    active, _ = g.branch(measured, g.math('BoolAnd', A=g.get('ClearanceRunning'), B=g.get('ClearanceValid')))
    norm = 0
    for fi in range(4):
        norm = scalar(g, 'Add', norm, scalar(g, 'Mul', g.at('ClearanceGradient', fi), g.at('ClearanceGradient', fi)))
    active = g.set(active, 'GradientNorm', norm)
    active = g.set(active, 'ClearanceRunning', scalar(g, 'GreaterEqual', g.get('GradientNorm'), 1e-8))
    active, _ = g.branch(active, g.get('ClearanceRunning'))
    multiplier = scalar(g, 'Div', scalar(g, 'Sub', model['margin_cm'], g.get('WorstGap')), scalar(g, 'Max', g.get('GradientNorm'), 1e-8))
    magnitude = 0
    for fi in range(4):
        magnitude = scalar(g, 'Max', magnitude, g.math('FloatAbs', Value=scalar(g, 'Mul', multiplier, g.at('ClearanceGradient', fi))))
    factor = scalar(g, 'Mul', multiplier, scalar(g, 'Min', 1, scalar(g, 'Div', math.radians(model['iteration_step_degrees']), scalar(g, 'Max', magnitude, 1e-12))))
    active = g.set(active, 'StepFactor', factor)
    limit = math.radians(model['correction_limit_degrees'])
    for fi in range(4):
        angle = scalar(g, 'Add', g.at('ClearanceAngles', fi), scalar(g, 'Mul', g.get('StepFactor'), g.at('ClearanceGradient', fi)))
        active = g.put(active, 'ClearanceAngles', fi, g.math('FloatClamp', Value=angle, Minimum=-limit, Maximum=limit))
    g.set(active, 'ClearanceIterations', g.math('IntAdd', A=g.get('ClearanceIterations'), B=1))
    # Export intended rotations independently of hierarchy write tolerances.
    array(g, 'ClearanceOutput', 'FTransform', ['()']*4)
    for fi in range(4):
        offset = g.math('QuaternionFromAxisAndAngle', Axis=g.at('ClearanceAxes', fi), Angle=g.at('ClearanceAngles', fi))
        q = g.math('QuaternionUnit', Value=g.math('QuaternionMul', A=offset, B=g.at('ClearanceLocals', fi*3, 'Rotation')))
        q = select(g, g.get('ClearanceValid'), q, g.at('ClearanceLocals', fi*3, 'Rotation'), 'Quaternion')
        output = g.math('TransformMake', Translation=g.at('ClearanceLocals', fi*3, 'Translation'), Rotation=q, Scale=g.at('ClearanceLocals', fi*3, 'Scale3D'))
        completed = g.put(completed, 'ClearanceOutput', fi, output)
    return completed

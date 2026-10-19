"""Explicit angular step shared by the timed body probe.

Copied from CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools/
controlrig_body_step.py (sha256 40a8326c...6d6) and extended for BodyAngularV11:
the travel input (RegionTravel[7] / Travel, degrees) and the soft limit
(SoftZone / RegionSoftZones[7]) from
docs/research/2026-10-19-body-motion-travel-channel.md. With
travel_input and soft off, it builds the same nodes as the V10 step.

State is owned by the graph. A partial display step is safe to discard only when
Orientation, Omega, LinearVelocity, PreviousTargets and PreviousStepSeconds are
all restored to their committed values by the scheduler.
"""
import math
from build_controlrig_body_probe import loop, log_rotation, difference, qmul, exp_rotation

# Soft limit constants from the design. ZONE_DAMPING is zetaL. The stiffness
# wL = sqrt(1.5) * wn / SoftZone stops a helper moving at the main spring's
# full-amplitude speed (2 pi f L) before L; it is capped at 1.5 / h.
ZONE_DAMPING = .7
ZONE_STIFFNESS_CAP = 1.5 * 120
ZONE_MAXIMUM = .9
# Cubic push (4/3) wL^2 d^3 / w^2: the 4/3 keeps the quadratic's energy to the limit.
PUSH_SCALE = 4/3
# Default band width, a fraction of the limit (Fable, 19 Oct 2026). The
# Earthquake template asks for 0.6 through the per-region input.
SOFT_ZONE_DEFAULT = .5
TRAVEL_CEILING_DEGREES = 45


def authored_limit(g, i, travel, scale):
    """Today's cap in radians: Limits[i], or the MaxTravel skin cap when smaller."""
    limit = g.at('Limits', i)
    if travel:
        maximum_scale = g.math('FloatMax', A=g.at('Targets', i, 'Scale3D.X'), B=g.math('FloatMax',
            A=g.at('Targets', i, 'Scale3D.Y'), B=g.at('Targets', i, 'Scale3D.Z')))
        radius = g.math('FloatMul', A=g.at('WeightedSupportRadii', i), B=maximum_scale)
        distance = g.math('FloatMul', A=g.math('FloatMax', A=0, B=g.at('MaxTravel', i)), B=scale)
        ratio = g.math('FloatClamp', Value=g.math('FloatDiv', A=distance,
            B=g.math('FloatMax', A=1e-6, B=g.math('FloatMul', A=2, B=radius))), Minimum=0, Maximum=1)
        travel_limit = g.math('FloatMul', A=2, B=g.math('FloatAsin', Value=ratio))
        limit = g.math('FloatMin', A=limit, B=travel_limit)
    return limit


def requested_limit(g, i, authored, regions):
    """A positive RegionTravel/Travel (degrees) replaces the authored cap; 0 keeps it."""
    requested = (g.math('FloatSelectBool', Condition=g.get('UseRegionSettings'),
        IfTrue=g.at('RegionTravel', i), IfFalse=g.get('Travel')) if regions else g.get('Travel'))
    radians = g.math('FloatMul', A=g.math('FloatMin', A=requested, B=TRAVEL_CEILING_DEGREES), B=math.pi/180)
    return g.math('FloatSelectBool', Condition=g.math('FloatGreater', A=requested, B=0),
        IfTrue=radians, IfFalse=authored)


def soft_zone(g, old, target, omega, target_omega, force, limit, wn, dt, zone):
    """Spring-back band over the outer SoftZone fraction of the limit.

    Against the committed orientation: theta = |log(old * target^-1)|, u its axis,
    s = (1 - z) L, w = z L. Above s a restoring acceleration
    (4/3) wL^2 (theta - s)^3 / w^2 pushes back along -u. It is zero, with zero
    slope and zero curvature, at the knee, and absorbs wL^2 w^2 / 3 (per unit
    inertia) between knee and limit: the same energy budget as the first
    design's quadratic push, which this cubic replaces (Fable, 19 Oct 2026), so
    the wL sizing still stops a full-amplitude swing before L. The outward
    component of the relative angular velocity is damped at 2 zetaL wL x, where
    x is the depth fraction. That damping is integrated exactly over the step
    (the same exponential form the main damping uses), so a large wL dt cannot
    reverse the velocity. A zone of 0 selects the incoming omega and force
    unchanged, which is today's hard clamp exactly. `zone` is the SoftZone
    input, per region when region settings are on.
    """
    zone = g.math('FloatClamp', Value=zone, Minimum=0, Maximum=ZONE_MAXIMUM)
    active = g.math('FloatGreater', A=zone, B=0)
    relative = log_rotation(g, difference(g, old, target))
    theta = g.length(relative)
    axis = g.direction(relative, (1, 0, 0))
    width = g.math('FloatMax', A=g.math('FloatMul', A=zone, B=limit), B=1e-6)
    knee = g.math('FloatSub', A=limit, B=width)
    # Bounded at the band width: a limit that shrinks under a committed state
    # does not produce an unbounded push; the hard projection handles the rest.
    depth = g.math('FloatClamp', Value=g.math('FloatSub', A=theta, B=knee), Minimum=0, Maximum=width)
    fraction = g.math('FloatDiv', A=depth, B=width)
    stiffness = g.math('FloatMin', A=g.math('FloatDiv', A=g.math('FloatMul', A=wn, B=math.sqrt(1.5)),
        B=g.math('FloatMax', A=zone, B=1e-6)), B=ZONE_STIFFNESS_CAP)
    outward = g.math('FloatMax', A=0, B=g.math('VectorDot', A=g.sub(omega, target_omega), B=axis))
    retained = g.math('FloatExponential', Value=g.math('FloatMul', A=g.math('FloatMul',
        A=g.math('FloatMul', A=stiffness, B=-2*ZONE_DAMPING), B=fraction), B=dt))
    damped = g.sub(omega, g.scale(axis, g.math('FloatMul', A=outward, B=g.math('FloatSub', A=1, B=retained))))
    cube = g.math('FloatMul', A=g.math('FloatMul', A=depth, B=depth), B=depth)
    push = g.math('FloatDiv', A=g.math('FloatMul', A=g.math('FloatMul', A=stiffness, B=stiffness),
        B=g.math('FloatMul', A=cube, B=PUSH_SCALE)), B=g.math('FloatMul', A=width, B=width))
    restored = g.sub(force, g.scale(axis, push))
    omega = g.math('VectorSelectBool', Condition=active, IfTrue=damped, IfFalse=omega)
    force = g.math('VectorSelectBool', Condition=active, IfTrue=restored, IfFalse=force)
    return omega, force


def step(g, start, committed=True, travel=False, planes=False, scaled_acceleration=False, contacts=False,
         regions=False, travel_input=False, soft=False):
    i, body, advance = loop(g, start)
    dt = g.get("StepSeconds")
    inverse_dt = g.math("FloatDiv", A=1, B=dt)
    target = g.at('Targets', i, 'Rotation')
    old = g.at('Orientation', i)
    raw_linear = g.scale(g.sub(g.at('Targets', i, 'Translation'), g.at('PreviousTargets', i, 'Translation')), inverse_dt)
    # Piecewise-linear animation samples produce velocity steps at each render
    # update. Filter on the simulation clock before differentiating, so a coarse
    # render frame does not concentrate all inertial force in one substep.
    # The exponential factor preserves the same 25 ms time constant in previews.
    fraction = g.math('FloatSub', A=1, B=g.math('FloatExponential',
        Value=g.math('FloatMul', A=dt, B=-1/.025)))
    linear = g.add(g.at('LinearVelocity', i), g.scale(g.sub(raw_linear, g.at('LinearVelocity', i)), fraction))
    # Keep the inertial guard in owner-local units, like travel and plane limits.
    # A fixed world cap clips equivalent motion earlier on a larger character.
    acceleration_limit = (g.math('FloatMul', A=5000, B=g.get('OwnerTransform', 'Scale3D.X'))
                          if scaled_acceleration else 5000)
    acceleration = g.math('VectorClampLength', Value=g.scale(g.sub(linear, g.at('LinearVelocity', i)), inverse_dt), MaximumLength=acceleration_limit)
    target_omega = g.scale(log_rotation(g, difference(g, target, g.at('PreviousTargets', i, 'Rotation'))), inverse_dt)
    setting = lambda name, array: (g.math('FloatSelectBool', Condition=g.get('UseRegionSettings'),
        IfTrue=g.at(array, i), IfFalse=g.get(name)) if regions else g.get(name))
    frequency = g.math('FloatClamp', Value=setting('Frequency', 'RegionFrequencies'), Minimum=.2, Maximum=10)
    damping = g.math('FloatClamp', Value=setting('DampingRatio', 'RegionDampingRatios'), Minimum=.01, Maximum=3)
    wn = g.math('FloatMul', A=frequency, B=2*math.pi)
    decay = g.math('FloatExponential', Value=g.math('FloatMul', A=g.math('FloatMul', A=wn, B=damping), B=g.math("FloatMul", A=dt, B=-2)))
    omega = g.add(target_omega, g.scale(g.sub(g.at('Omega', i), target_omega), decay))
    error = log_rotation(g, difference(g, target, old))
    force = g.scale(error, g.math('FloatMul', A=wn, B=wn))
    # Isotropic center-of-mass inertia plus parallel-axis contribution. The
    # acceleration torque is perpendicular to the offset, so this denominator
    # is the exact inverse moment for this deliberately isotropic body model.
    scale = g.get('OwnerTransform', 'Scale3D.X')
    offset = g.math('QuaternionRotateVector', Transform=old,
                    Vector=g.math('VectorMul', A=g.at('Offsets', i), B=g.at('Targets', i, 'Scale3D')))
    acceleration = g.scale(acceleration, g.math('FloatClamp', Value=setting('MotionAmount', 'RegionMotionAmounts'), Minimum=0, Maximum=5))
    torque = g.math('VectorCross', A=offset, B=g.sub(g.get('Gravity'), acceleration))
    force = g.add(force, g.scale(torque, g.math('FloatDiv', A=1, B=g.math('FloatMul', A=g.at('Moments', i), B=g.math('FloatMul', A=scale, B=scale)))))
    limit = authored_limit(g, i, travel, scale)
    if travel_input:
        limit = requested_limit(g, i, limit, regions)
    if soft:
        omega, force = soft_zone(g, old, target, omega, target_omega, force, limit, wn, dt,
                                 setting('SoftZone', 'RegionSoftZones'))
    omega = g.add(omega, g.scale(force, dt))
    proposed = qmul(g, exp_rotation(g, g.scale(omega, dt)), old)
    relative = log_rotation(g, difference(g, proposed, target))
    length = g.length(relative)
    clipped = g.math('FloatGreater', A=length, B=limit)
    axis = g.direction(relative, (1, 0, 0))
    bounded = g.scale(axis, g.math('FloatMin', A=length, B=limit))
    orientation = qmul(g, exp_rotation(g, bounded), target)
    outward = g.math('FloatMax', A=0, B=g.math('VectorDot', A=g.sub(omega, target_omega), B=axis))
    corrected = g.sub(omega, g.scale(axis, outward))
    omega = g.math('VectorSelectBool', Condition=clipped, IfTrue=corrected, IfFalse=omega)
    if planes:
        from controlrig_body_planes import project
        body, orientation, omega = project(g, body, i, target, bounded, omega, target_omega, contacts=contacts)
    if regions:
        enabled = g.math('BoolOr', A=g.math('BoolNot', Value=g.get('UseRegionSettings')), B=g.at('RegionEnabled', i))
        orientation = g.math('QuaternionSelectBool', Condition=enabled, IfTrue=orientation, IfFalse=target)
        omega = g.math('VectorSelectBool', Condition=enabled, IfTrue=omega, IfFalse=target_omega)
    if soft and committed:
        # Count committed helper steps where the hard projection still engaged,
        # before any state is replaced (pure inputs read the old orientation).
        hit = clipped
        if regions:
            hit = g.math('BoolAnd', A=hit, B=enabled)
        body = g.set(body, 'ClampContacts', g.math('IntAdd', A=g.get('ClampContacts'),
            B=g.math('FloatFloor', output='Int', Value=g.math('FloatSelectBool', Condition=hit, IfTrue=1, IfFalse=0))))
    # Materialize every next-state expression before replacing any old state.
    # Pure RigVM expressions can be reevaluated at a later mutable statement.
    body = g.set(body, 'NextOmega', omega)
    body = g.set(body, 'NextOrientation', orientation)
    body = g.set(body, 'NextLinear', linear)
    body = g.put(body, 'Omega', i, g.get('NextOmega'))
    body = g.put(body, 'Orientation', i, g.get('NextOrientation'))
    g.put(body, 'LinearVelocity', i, g.get('NextLinear'))
    advance = g.set(advance, 'PreviousTargets', g.get('Targets'))
    advance = g.set(advance, 'PreviousStepSeconds', dt)
    if committed:
        advance = g.set(advance, 'TotalSteps', g.math('IntAdd', A=g.get('TotalSteps'), B=1))
    return advance

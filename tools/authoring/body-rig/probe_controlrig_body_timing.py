"""Exercise the saved timed body graph with real owner transforms and frame times.

Copied from the SeduXtress authoring library (probe_controlrig_body_timing.py,
sha256 05e20059...4d6) and extended for BodyAngularV11: the travel inputs,
SoftZone, the ClampContacts counter, per-region tuning, a precise per-frame
angle and limit, extra motions and lean samples for the soft-limit fixtures.
The 37 original fixtures and their sample format are unchanged.

-CSSBodyVersion=10|11, -CSSBodySoftZone=<z> (V11, default 0.5, written to
SoftZone and to all seven RegionSoftZones),
-CSSBodyLabel=<suffix> for the output name. Output goes to CSS_BODY_RIG_STAGE.
"""
import hashlib
import json
import math
import re
import shlex
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from body_rig_paths import MEASURED, stage
from probe_controlrig_chain import key, set_value, transform
from probe_controlrig_world import multiply, rotate
from probe_controlrig_body import angle, quaternion

ARGS = shlex.split(unreal.SystemLibrary.get_command_line())
VERSION = int(next((a.split('=',1)[1] for a in ARGS if a.startswith('-CSSBodyVersion=')), '4'))
if VERSION not in (2, 3, 4, 5, 6, 7, 8, 9, 10, 11):
    raise ValueError('Unknown timed body version')
PACKAGE = f'/Game/CSSAuthoring/TransientProbes/CR_CSS_BodyAngularV{VERSION}'
STATE = ('Orientation', 'Omega', 'LinearVelocity', 'PreviousTargets', 'PreviousStepSeconds')
SOFT_ZONE = float(next((a.split('=',1)[1] for a in ARGS if a.startswith('-CSSBodySoftZone=')), '.5'))
LABEL = next((a.split('=',1)[1] for a in ARGS if a.startswith('-CSSBodyLabel=')), '')
if not re.fullmatch(r'[-a-z0-9]*', LABEL):
    raise ValueError('Label must be lowercase letters, digits and dashes')


def floats(text):
    return [float(v) for v in text.strip('()').split(',')]


def relative_angle(a, b):
    """Angle between two rotations in radians, accurate near zero."""
    x,y,z,w = a
    X,Y,Z,W = b
    # conjugate(a) * b
    q = [w*X-x*W-y*Z+z*Y, w*Y+x*Z-y*W-z*X, w*Z-x*Y+y*X-z*W, w*W+x*X+y*Y+z*Z]
    return 2*math.atan2(math.sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]), abs(q[3]))


def fingerprint(rig):
    return hashlib.sha256('\n'.join(rig.get_variable_as_string(name) for name in STATE).encode()).hexdigest()


def evaluate(bp, names, case):
    component = unreal.SceneComponent()
    rig = bp.create_control_rig()
    if not rig.rename(outer=component):
        raise RuntimeError('Cannot establish body rig owner')
    hierarchy = rig.get_hierarchy()
    region_settings = None
    if VERSION >= 10:
        region_settings = dict(RegionFrequencies=[2.]*7, RegionDampingRatios=[.7]*7,
                               RegionMotionAmounts=[1.]*7, RegionEnabled=[True]*7)
        set_value(rig, 'UseRegionSettings', case.get('region_settings', False))
        if 'region_field' in case:
            region_settings[case['region_field']][case['region_index']] = case['region_value']
        for name, values in case.get('tuning', {}).items():
            if name not in region_settings or len(values) != 7:
                raise ValueError('Unknown or partial region tuning: ' + name)
            region_settings[name] = list(values)
        if case.get('regions_off'):
            region_settings['RegionEnabled'] = [False]*7
        for name, values in region_settings.items():
            set_value(rig, name, '('+','.join(str(v) for v in values)+')')
    travel_inputs = None
    if VERSION >= 11:
        travel_inputs = dict(soft_zone=float(case.get('soft_zone', SOFT_ZONE)),
                             region_travel=[float(v) for v in case.get('region_travel', [0.]*7)],
                             travel=float(case.get('travel', 0.)))
        if len(travel_inputs['region_travel']) != 7:
            raise ValueError('Expected seven region travel values')
        set_value(rig, 'SoftZone', travel_inputs['soft_zone'])
        # The per-region zone is read when region settings are on; both carry
        # the case's zone so either mode runs the requested band.
        set_value(rig, 'RegionSoftZones', '('+','.join(repr(travel_inputs['soft_zone']) for _ in range(7))+')')
        set_value(rig, 'RegionTravel', '('+','.join(repr(v) for v in travel_inputs['region_travel'])+')')
        set_value(rig, 'Travel', travel_inputs['travel'])
        travel_inputs['read_back'] = dict(SoftZone=rig.get_variable_as_string('SoftZone'),
            RegionSoftZones=rig.get_variable_as_string('RegionSoftZones'),
            RegionTravel=rig.get_variable_as_string('RegionTravel'), Travel=rig.get_variable_as_string('Travel'))
    authored = None
    if VERSION >= 5:
        authored = {name: floats(rig.get_variable_as_string(name)) for name in ('Limits', 'MaxTravel', 'WeightedSupportRadii')}
    region_mode = bool(case.get('region_settings', False)) if VERSION >= 10 else False
    contacts = None
    if VERSION >= 9:
        from body_contact_probe_inputs import setup
        contacts = setup(rig, hierarchy, names, case)
    planes = None
    if VERSION >= 6:
        planes = dict(enabled=case.get('planes_enabled', True))
        set_value(rig, 'PlanesEnabled', planes['enabled'])
        for suffix in ('Lateral', 'Depth'):
            name = 'Plane' + suffix + 'Normals'
            if case.get('plane_zero_normals'):
                set_value(rig, name, '('+','.join('(X=0,Y=0,Z=0)' for _ in names)+')')
            values = [float(v) for v in re.findall(r'[XYZ]=([-+0-9.eE]+)', rig.get_variable_as_string(name))]
            if len(values) != 3*len(names):
                raise ValueError('Unexpected plane normal array')
            planes[name] = [values[i:i+3] for i in range(0,len(values),3)]
            name = 'Plane' + suffix + 'Clearance'
            if 'plane_clearance' in case:
                set_value(rig, name, '('+','.join(str(case['plane_clearance']) for _ in names)+')')
            planes[name] = [float(v) for v in rig.get_variable_as_string(name).strip('()').split(',')]
    initial = {name: transform(hierarchy.get_global_transform(key(name), True)) for name in names}
    base = transform(hierarchy.get_global_transform(key('pelvis'), True))
    inverse = [-v for v in base['rotation'][:3]]+base['rotation'][3:]
    fps = case.get('fps', 60)
    times = [i/fps for i in range(int(case.get('seconds', 4))*fps+1)]
    if case.get('jitter'):
        times = [0.]
        for i in range(120):
            times.extend([i/30+1/90, (i+1)/30])
    samples = []
    maximum = dict(pivot_cm=0., scale=0., angle_degrees=0., limit_excess_degrees=0.,
                   quaternion_norm_error=0., substeps=0, preview_steps=0)
    preview_unchanged = 0
    limits = [6, 6, 10, 10, 3, 3, 5]
    for frame, t in enumerate(times):
        dt = t-times[frame-1] if frame else 0
        hierarchy.reset_pose_to_initial(unreal.RigElementType.BONE)
        contact_epoch = 0
        if case.get('contact_morph_change'):
            contact_epoch = 0 if t < 1 else 1 if t < 2 else 2
            if frame == 0 or contact_epoch != previous_contact_epoch:
                from body_contact_probe_inputs import setup
                contacts = setup(rig, hierarchy, names, dict(case, contact_morphs=[float(contact_epoch == 1)]*6))
            previous_contact_epoch = contact_epoch
        phase = min(t, 2-t) if t < 2 else 0
        if case.get('smooth'):
            phase = math.sin(math.pi*t)**2 if t < 2 else 0
        if case.get('bounce'):
            # Running-like vertical bob: piecewise linear, 2.5 cycles per second.
            cycle = (t*case['bounce']) % 1
            phase = (1-abs(2*cycle-1)) if t < 2 else 0
        motion = case.get('motion', 'translation')
        move = [0., 0., 0.]
        if motion == 'translation':
            move[case.get('axis', 0)] = case.get('amplitude', 10)*phase
        q = quaternion(0, math.radians(20)*phase) if motion == 'parent' else [0., 0., 0., 1.]
        delta_q = multiply(multiply(base['rotation'], q), inverse)
        scale = case.get('scale', 1.)
        if case.get('scale_change'):
            scale = 1.5 if 1 <= t < 2 else 1.
        owner = unreal.Transform()
        owner.scale3d = unreal.Vector(scale, scale, scale)
        if case.get('nonuniform'):
            owner.scale3d = unreal.Vector(1, 2, 1)
        origin = case.get('origin', [0., 0., 0.])
        owner.translation = unreal.Vector(*origin)
        if case.get('component'):
            owner.rotation = unreal.Quat(*delta_q)
            rotated_pivot = rotate(delta_q, base['translation'])
            owner.translation = unreal.Vector(*[o+scale*(p-r+m) for o,p,r,m in zip(origin,base['translation'],rotated_pivot,move)])
        elif motion in ('translation', 'parent'):
            pelvis = hierarchy.get_global_transform(key('pelvis'), True)
            pelvis.translation = unreal.Vector(*[a+b for a,b in zip(base['translation'], move)])
            pelvis.rotation = unreal.Quat(*multiply(base['rotation'], q))
            hierarchy.set_global_transform(key('pelvis'), pelvis, False, True)
        if motion == 'local':
            for name in names:
                bone = hierarchy.get_global_transform(key(name))
                bone.rotation = unreal.Quat(*multiply(initial[name]['rotation'], quaternion(2, math.radians(20)*phase)))
                hierarchy.set_global_transform(key(name), bone, False, False)
        if case.get('contact_moving_peer'):
            bone = hierarchy.get_global_transform(key('thigh_r'))
            bone.translation += unreal.Vector(8*phase, 0, 0)
            hierarchy.set_global_transform(key('thigh_r'), bone, False, True)
        if case.get('teleport') and t >= 1:
            owner.translation += unreal.Vector(1000, -2000, 500)
        component.set_world_transform(owner, False, True)
        incoming = [transform(hierarchy.get_global_transform(key(name))) for name in names]
        contact_pose = [transform(hierarchy.get_global_transform(key(name))) for name in contacts['bones']] if contacts else None
        preserved = {name: transform(hierarchy.get_global_transform(key(name))) for name in
                     ('pelvis', 'spine_04', 'head', 'foot_l', 'foot_r', 'thigh_l', 'thigh_r')}
        enabled = not case.get('disabled') and not (case.get('reenable') and 1 <= t < 1.5)
        travel = case.get('max_travel')
        if case.get('travel_change'):
            travel = [.05]*7 if 1 <= t < 2 else [1.5,1.5,1,1,.5,.5,.5]
        if travel is not None:
            if len(travel)!=7 or not all(math.isfinite(v) for v in travel):
                raise ValueError('Expected seven finite travel limits')
            set_value(rig,'MaxTravel','('+','.join(str(v) for v in travel)+')')
        set_value(rig, 'Enabled', enabled)
        if case.get('region_toggle'):
            region_settings = dict(region_settings, RegionEnabled=[True]*7)
            region_settings['RegionEnabled'][case['region_index']] = not (1 <= t < 1.5)
            set_value(rig, 'RegionEnabled', '('+','.join(str(v) for v in region_settings['RegionEnabled'])+')')
        set_value(rig, 'UpdateSerial', frame)
        set_value(rig, 'ResetEpoch', int(bool(case.get('reset')) and t >= 1) + contact_epoch)
        if abs(t-1) < 1e-8 and case.get('hitch'):
            dt = .2
        if abs(t-1) < 1e-8 and case.get('zero_delta'):
            dt = 0
        before, resets = fingerprint(rig), int(rig.get_variable_as_string('ResetCount'))
        rig.set_delta_time(dt)
        if not rig.execute('Forwards Solve'):
            raise RuntimeError('Timed body rig execution failed')
        after = fingerprint(rig)
        substeps = int(rig.get_variable_as_string('Substeps'))
        previews = int(rig.get_variable_as_string('PreviewSteps'))
        new_resets = int(rig.get_variable_as_string('ResetCount'))
        if frame and enabled and substeps == 0 and previews and resets == new_resets:
            if before != after:
                raise RuntimeError('Partial preview changed committed angular/derivative state')
            preview_unchanged += 1
        outgoing = [transform(hierarchy.get_global_transform(key(name))) for name in names]
        for name, value in preserved.items():
            if transform(hierarchy.get_global_transform(key(name))) != value:
                raise RuntimeError('Timed body rig changed a non-helper bone')
        errors = []
        thetas, bounds = [], []
        for i,(a,b) in enumerate(zip(incoming,outgoing)):
            thetas.append(relative_angle(a['rotation'], b['rotation']))
            if authored:
                requested = 0.
                if travel_inputs:
                    requested = travel_inputs['region_travel'][i] if region_mode else travel_inputs['travel']
                if requested > 0:
                    bounds.append(math.radians(min(requested, 45)))
                else:
                    max_travel = (travel if travel is not None else authored['MaxTravel'])[i]
                    ratio = max(0., max_travel)/max(1e-6, 2*authored['WeightedSupportRadii'][i]*max(a['scale']))
                    bounds.append(min(authored['Limits'][i], 2*math.asin(min(1., ratio))))
            if not all(math.isfinite(v) for values in b.values() for v in values):
                raise RuntimeError('Non-finite body output')
            error = angle(a['rotation'], b['rotation'])
            errors.append(error)
            maximum['pivot_cm'] = max(maximum['pivot_cm'], math.dist(a['translation'], b['translation']))
            maximum['scale'] = max(maximum['scale'], math.dist(a['scale'], b['scale']))
            maximum['angle_degrees'] = max(maximum['angle_degrees'], error)
            maximum['limit_excess_degrees'] = max(maximum['limit_excess_degrees'], error-limits[i])
            maximum['quaternion_norm_error'] = max(maximum['quaternion_norm_error'], abs(sum(v*v for v in b['rotation'])-1))
        clamp_contacts = int(rig.get_variable_as_string('ClampContacts')) if VERSION >= 11 else None
        maximum['substeps'] = max(maximum['substeps'], substeps)
        maximum['preview_steps'] = max(maximum['preview_steps'], previews)
        if case.get('duplicate'):
            if not rig.execute('Forwards Solve') or after != fingerprint(rig):
                raise RuntimeError('Duplicate evaluation advanced committed state')
        if case.get('lean'):
            samples.append(dict(frame=frame,time=t,delta=dt,enabled=enabled,resets=new_resets,
                steps=int(rig.get_variable_as_string('TotalSteps')),substeps=substeps,previews=previews,
                remainder=float(rig.get_variable_as_string('Remainder')),angles_degrees=errors,
                theta_radians=thetas,limit_radians=bounds,clamp_contacts=clamp_contacts,
                rotations=[multiply(transform(owner)['rotation'],b['rotation']) for b in outgoing]))
            continue
        samples.append(dict(frame=frame,time=t,delta=dt,enabled=enabled,resets=new_resets,
            steps=int(rig.get_variable_as_string('TotalSteps')),substeps=substeps,previews=previews,
            remainder=float(rig.get_variable_as_string('Remainder')), owner=transform(owner),
            incoming=incoming,body_pose=[dict(name=n,**v) for n,v in zip(names,outgoing)],
            parent_pose=[dict(name=n,**v) for n,v in preserved.items()],angles_degrees=errors,
            max_travel=travel,contact_pose=contact_pose,
            contact_inputs=contacts if case.get('contact_morph_change') else None,
            region_enabled=region_settings['RegionEnabled'] if region_settings else None,
            theta_radians=thetas,limit_radians=bounds,clamp_contacts=clamp_contacts))
    return dict(**case,maximum=maximum,preview_unchanged_frames=preview_unchanged,samples=samples,planes=planes,
                contacts=contacts,travel_inputs=travel_inputs,authored_limits=authored)


def main():
    selection = next((a.split('=',1)[1] for a in ARGS if a.startswith('-CSSBodyCases=')), '')
    output = stage()/f'controlrig-body-angular-v{VERSION}-timing{"-"+LABEL if LABEL else ""}{"-selected" if selection else ""}.json'
    if output.exists():
        raise FileExistsError(output)
    bp = unreal.load_asset(PACKAGE)
    if bp is None:
        raise RuntimeError('Saved timed body rig is missing')
    names = [h['bone'] for h in json.loads((MEASURED/'body-helper-layout-v1.json').read_text())['helpers']]
    cases = [dict(name=f'{motion}_{fps}',motion=motion,fps=fps) for motion in ('translation','local','parent')
             for fps in (15,30,60,120,144,240)]
    cases += [dict(name=f'smooth_{fps}',fps=fps,smooth=True) for fps in (30,60,120,144,240)]
    cases += [dict(name='jitter',jitter=True), dict(name='component_translation',component=True),
              dict(name='component_parent',motion='parent',component=True),
              dict(name='far_origin',origin=[100000,-200000,30000]),
              dict(name='uniform_scale',scale=1.5,smooth=True),
              dict(name='nonuniform_passthrough',nonuniform=True),dict(name='scale_change',scale_change=True),
              dict(name='teleport_automatic',teleport=True),dict(name='epoch_reset',motion='local',reset=True),
              dict(name='hitch',hitch=True),dict(name='zero_delta',zero_delta=True),
              dict(name='reenable',reenable=True),dict(name='duplicate',fps=144,duplicate=True),
              dict(name='disabled',disabled=True)]
    if selection:
        requested = selection.split(',')
        if len(set(requested)) != len(requested) or not set(requested).issubset(c['name'] for c in cases):
            raise ValueError('Unknown or repeated body timing case')
        cases = [c for c in cases if c['name'] in requested]
    results = []
    for case in cases:
        results.append(evaluate(bp,names,case))
        unreal.log('Timed body case completed: '+case['name'])
    # One write at the end: the per-case rewrite of the original grows with n squared.
    output.write_text(json.dumps(dict(package=PACKAGE,helpers=names,cases=results,
        soft_zone=SOFT_ZONE if VERSION >= 11 else None,
        scope='Direct timed rig measurements. No contacts, AnimBP, cooked or live acceptance.'),indent=2)+'\n')


if __name__ == '__main__':
    main()

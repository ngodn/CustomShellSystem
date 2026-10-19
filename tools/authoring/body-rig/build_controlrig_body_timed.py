"""Author a fixed-clock body rig with a reversible partial display step.

Copied from the SeduXtress authoring library (build_controlrig_body_timed.py,
sha256 cb145424...e969). -CSSBodyTravelInput authors BodyAngularV11: V10 plus
RegionTravel[7]/Travel (degrees, 0 keeps Limits/MaxTravel), SoftZone (default
0.25, 0 is the hard clamp) and the ClampContacts counter. The report goes to
CSS_BODY_RIG_STAGE; the saved rig goes to the editor project's content.
"""
import json
import math
import shlex
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from body_rig_paths import HERE, MEASURED, stage
# Before the library helpers: build_controlrig_body_probe puts the library
# directory first on sys.path, which would shadow this copy of the step.
import controlrig_body_step
from controlrig_body_step import step
from build_controlrig_chain_probe import Graph, Pin, TYPES
from build_controlrig_body_probe import H, COUNT, MESH, loop, difference, log_rotation

if Path(controlrig_body_step.__file__).resolve().parent != HERE:
    raise ImportError('Expected the travel/soft-limit step from ' + str(HERE))

ARGS = shlex.split(unreal.SystemLibrary.get_command_line())
VERSION = 11 if '-CSSBodyTravelInput' in ARGS else 10 if '-CSSBodyRegions' in ARGS else 9 if '-CSSBodyContacts' in ARGS else 8 if '-CSSBodyPlanes' in ARGS else 5 if '-CSSBodyTravel' in ARGS else 4
PACKAGE = f'/Game/CSSAuthoring/TransientProbes/CR_CSS_BodyAngularV{VERSION}'
# -CSSBodyHierarchyFrom=<rig>: start from a copy of that rig's hierarchy (see main).
HIERARCHY_FROM = next((a.split('=', 1)[1] for a in ARGS if a.startswith('-CSSBodyHierarchyFrom=')), '')
STATE = ('Orientation', 'Omega', 'LinearVelocity', 'PreviousTargets')


def output_pose(g, start):
    i, body, end = loop(g, start)
    node = g.unit('RigUnit_SetTransform', Space='GlobalSpace', bInitial=False, bPropagateToChildren=False)
    g.link(body, node+'.ExecuteContext')
    g.value(node+'.Item.Type', 'Bone')
    g.value(node+'.Item.Name', g.at('BoneNames', i))
    g.value(node+'.Value.Translation', g.at('RigTargets', i, 'Translation'))
    g.value(node+'.Value.Scale3D', g.at('RigTargets', i, 'Scale3D'))
    g.value(node+'.Value.Rotation', Pin(g.unit('RigUnit_ToRigSpace_Rotation', Value=g.at('Display', i))+'.Global'))
    return end


def finish(g, start):
    if VERSION >= 9:
        start = g.set(start, 'ContactPrevious', g.get('ContactCurrent'))
    for name, value in [('FramePrevious', g.get('FrameTargets')), ('Initialized', True),
                        ('SeenEpoch', g.get('ResetEpoch')), ('SeenUpdate', g.get('UpdateSerial'))]:
        start = g.set(start, name, value)
    output_pose(g, start)


def skip(g, start):
    for name, value in [('Initialized', False), ('Remainder', 0), ('Substeps', 0),
                        ('PreviewSteps', 0), ('SeenUpdate', g.get('UpdateSerial'))]:
        start = g.set(start, name, value)


def main():
    report = stage()/f'controlrig-body-angular-v{VERSION}-build.json'
    if report.exists() or unreal.EditorAssetLibrary.does_asset_exist(PACKAGE):
        raise FileExistsError('Preserve the previous body rig')
    layout = json.loads((MEASURED/'body-helper-layout-v1.json').read_text())['helpers']
    if len(layout) != COUNT or any(h['descendants'] for h in layout):
        raise ValueError('Expected seven measured leaf helpers')
    if HIERARCHY_FROM:
        # The V40 mesh has since gained nine virtual bones, and neither a fresh
        # mesh import nor a text copy reproduces the saved hierarchy exactly.
        # Duplicating the earlier rig keeps its 379 bones and initial pose bit
        # for bit; its graph and members are then removed and authored anew.
        rig = unreal.EditorAssetLibrary.duplicate_asset(HIERARCHY_FROM, PACKAGE)
        if not isinstance(rig, unreal.ControlRigBlueprint):
            raise RuntimeError('Cannot duplicate the source body rig')
        rig.set_auto_vm_recompile(False)
        if len(rig.get_editor_property('hierarchy').get_bones()) != 379:
            raise ValueError('Expected unchanged 379-bone bind hierarchy')
        controller = rig.get_controller()
        for node in list(controller.get_graph().get_nodes()):
            if not controller.remove_node(node, setup_undo_redo=False):
                raise RuntimeError('Cannot clear the duplicated graph')
        if controller.get_graph().get_nodes():
            raise RuntimeError('Duplicated graph was not cleared')
        # Members are kept with their saved defaults. Several were computed
        # from the hierarchy and written with 12 digits at the time; rebuilding
        # them today changes the chest by one float ulp, so SoftZone 0 would no
        # longer reproduce the source rig exactly. New members are added below.
        kept = {str(v.name): str(v.cpp_type) for v in rig.get_member_variables()}
    else:
        rig = unreal.ControlRigBlueprintFactory.create_new_control_rig_asset(PACKAGE)
        rig.set_auto_vm_recompile(False)
        mesh = unreal.load_asset(MESH)
        if len(rig.get_hierarchy_controller().import_bones_from_skeletal_mesh(mesh, 'None')) != 379:
            raise ValueError('Expected unchanged 379-bone bind hierarchy')
        rig.set_preview_mesh(mesh, False)
    TYPES['FQuat'] = '/Script/CoreUObject.Quat'
    g = Graph(rig)
    if HIERARCHY_FROM:
        add_member = g.member

        def member(name, kind, default, public=False):
            if name not in kept:
                return add_member(name, kind, default, public)
            element = kind[7:-1] if kind.startswith('TArray<') else kind
            if (element.lstrip('F') not in kept[name].replace('double', 'float')
                    or kind.startswith('TArray<') != kept[name].startswith('TArray<')):
                raise TypeError(f'Kept member {name} is {kept[name]}, expected {kind}')
            g.types[name] = kind
        g.member = member
    else:
        kept = {}
    for name, kind, default in [
        ('Initialized', 'bool', 'False'), ('Enabled', 'bool', 'True'),
        ('ValidScale', 'bool', 'True'), ('PoseChanged', 'bool', 'False'),
        ('ScaleChanged', 'bool', 'False'), ('Teleport', 'bool', 'False'),
        ('ResetEpoch', 'int32', '0'), ('SeenEpoch', 'int32', '-1'),
        ('UpdateSerial', 'int32', '0'), ('SeenUpdate', 'int32', '-1'),
        ('TotalSteps', 'int32', '0'), ('ResetCount', 'int32', '0'),
        ('Substeps', 'int32', '0'), ('PreviewSteps', 'int32', '0'),
        ('Remainder', 'float', '0'), ('OldRemainder', 'float', '0'),
        ('StepSeconds', 'float', str(H)), ('PreviousStepSeconds', 'float', str(H)),
        ('SavedPreviousStepSeconds', 'float', str(H)),
        ('Frequency', 'float', '2'), ('DampingRatio', 'float', '.7'),
        ('MotionAmount', 'float', '1'), ('Gravity', 'FVector', '(X=0,Y=0,Z=0)'),
        ('NextOrientation', 'FQuat', '(X=0,Y=0,Z=0,W=1)'),
        ('NextOmega', 'FVector', '(X=0,Y=0,Z=0)'), ('NextLinear', 'FVector', '(X=0,Y=0,Z=0)'),
        ('OwnerTransform', 'FTransform', '()'),
    ]:
        g.member(name, kind, default, True)
    pack = lambda entries: '('+','.join(entries)+')'
    vector = lambda v: '('+','.join(f'{a}={x:.12g}' for a, x in zip('XYZ', v))+')'
    limits = [6, 6, 10, 10, 3, 3, 5]
    arrays = [('BoneNames', 'FName', pack('"'+h['bone']+'"' for h in layout)),
              ('Offsets', 'FVector', pack(vector(h['joint_offset_cm']) for h in layout)),
              ('Moments', 'float', pack(str(h['isotropic_box_dimension_cm']**2/6+h['pivot_to_center_cm']**2) for h in layout)),
              ('Limits', 'float', pack(str(math.radians(v)) for v in limits))]
    if VERSION >= 5:
        support = json.loads((MEASURED/'body-travel-envelope-v41.json').read_text())['helpers']
        if [h['bone'] for h in support] != [h['bone'] for h in layout]:
            raise ValueError('Travel envelope helper order differs')
        arrays += [('WeightedSupportRadii','float',pack(str(h['weighted_radius_cm']+.01) for h in support)),
                   ('MaxTravel','float',pack(map(str,[1.5,1.5,1,1,.5,.5,.5])))]
    if VERSION >= 6:
        from probe_controlrig_world import rotate
        from probe_controlrig_chain import key
        lateral, depth = [], []
        for helper in layout:
            transform = rig.get_editor_property('hierarchy').get_global_transform(key(helper['bone']), True)
            q = transform.rotation
            inverse = [-q.x, -q.y, -q.z, q.w]
            center = helper['surface_weighted_center_cm']
            # Character component X separates left/right, Y is front/back.
            # Belly has no lateral side. Normals point away from the midline
            # and torso, and are stored in each helper's incoming pose frame.
            lateral.append(rotate(inverse, [1 if center[0] > 0 else -1, 0, 0])
                           if helper['bone'] != 'belly' else [0, 0, 0])
            depth.append(rotate(inverse, [0, 1 if center[1] > 0 else -1, 0]))
        arrays += [('PlaneLateralNormals', 'FVector', pack(map(vector, lateral))),
                   ('PlaneDepthNormals', 'FVector', pack(map(vector, depth))),
                   ('PlaneLateralClearance', 'float', pack(map(str, [.25,.25,.15,.15,.025,.025,.05]))),
                   ('PlaneDepthClearance', 'float', pack(map(str, [.25,.25,.15,.15,.025,.025,.05])))]
        g.member('PlanesEnabled', 'bool', 'True', True)
    arrays += [(name, 'FQuat', pack('(X=0,Y=0,Z=0,W=1)' for _ in layout))
               for name in ('Orientation', 'SavedOrientation', 'Display')]
    arrays += [(name, 'FVector', pack('(X=0,Y=0,Z=0)' for _ in layout))
               for name in ('Omega', 'SavedOmega', 'LinearVelocity', 'SavedLinearVelocity')]
    arrays += [(name, 'FTransform', pack('()' for _ in layout))
               for name in ('Targets', 'PreviousTargets', 'SavedPreviousTargets', 'RigTargets', 'FrameTargets', 'FramePrevious')]
    for name, kind, default in arrays:
        g.member(name, f'TArray<{kind}>', default, True)
    if VERSION >= 9:
        from controlrig_body_contacts import configure
        configure(g)
    if VERSION >= 10:
        g.member('UseRegionSettings', 'bool', 'False', True)
        for name, kind, value in [('RegionFrequencies', 'float', '2'),
                                  ('RegionDampingRatios', 'float', '.7'),
                                  ('RegionMotionAmounts', 'float', '1'),
                                  ('RegionEnabled', 'bool', 'True')]:
            g.member(name, f'TArray<{kind}>', pack(value for _ in layout), True)
    if VERSION >= 11:
        # Degrees; 0 keeps the authored Limits/MaxTravel cap. SoftZone 0 is the
        # V10 hard clamp. Both are public members, so the Blueprint variable
        # defaults and the compiled CDO carry the same values.
        g.member('RegionTravel', 'TArray<float>', pack('0' for _ in layout), True)
        g.member('Travel', 'float', '0', True)
        g.member('SoftZone', 'float', '.25', True)
        g.member('ClampContacts', 'int32', '0', True)

    either = lambda a, b: g.math('BoolOr', A=a, B=b)
    both = lambda a, b: g.math('BoolAnd', A=a, B=b)
    changed = lambda kind, a, b: g.math(kind+'NotEquals', A=g.get(a), B=g.get(b))
    begin = Pin(g.unit('RigUnit_BeginExecution')+'.ExecuteContext')
    active, disabled = g.branch(begin, g.get('Enabled'))
    skip(g, disabled)
    active = g.set(active, 'OwnerTransform', Pin(g.unit('RigUnit_ToWorldSpace_Transform')+'.World'))
    owner_x = g.get('OwnerTransform', 'Scale3D.X')
    owner_uniform = both(
        g.math('FloatLess', A=g.math('FloatAbs', Value=g.math('FloatSub', A=owner_x, B=g.get('OwnerTransform', 'Scale3D.Y'))), B=1e-5),
        g.math('FloatLess', A=g.math('FloatAbs', Value=g.math('FloatSub', A=owner_x, B=g.get('OwnerTransform', 'Scale3D.Z'))), B=1e-5))
    for name, value in [('ValidScale', both(owner_uniform, g.math('FloatGreater', A=owner_x, B=1e-6))),
                        ('PoseChanged', False), ('ScaleChanged', False), ('Teleport', False)]:
        active = g.set(active, name, value)
    i, body, active = loop(g, active)
    node = g.unit('RigUnit_GetTransform', Space='GlobalSpace', bInitial=False)
    g.value(node+'.Item.Type', 'Bone')
    g.value(node+'.Item.Name', g.at('BoneNames', i))
    body = g.put(body, 'RigTargets', i, Pin(node+'.Transform'))
    body = g.put(body, 'FrameTargets', i, Pin(g.unit('RigUnit_ToWorldSpace_Transform', Value=g.at('RigTargets', i))+'.World'))
    if VERSION >= 9:
        from controlrig_body_contacts import gather
        body = gather(g, body, i)
    scale = g.at('FrameTargets', i, 'Scale3D')
    x = g.at('FrameTargets', i, 'Scale3D.X')
    # Authored bone scales contain small nonuniform values. Preserve them and
    # test uniformity on the owner, not on each accumulated bone transform.
    positive = both(g.math('FloatGreater', A=x, B=1e-6), both(
        g.math('FloatGreater', A=g.at('FrameTargets', i, 'Scale3D.Y'), B=1e-6),
        g.math('FloatGreater', A=g.at('FrameTargets', i, 'Scale3D.Z'), B=1e-6)))
    body = g.set(body, 'ValidScale', both(g.get('ValidScale'), positive))
    position_delta = g.length(g.sub(g.at('FrameTargets', i, 'Translation'), g.at('FramePrevious', i, 'Translation')))
    angle_delta = g.length(log_rotation(g, difference(g, g.at('FrameTargets', i, 'Rotation'), g.at('FramePrevious', i, 'Rotation'))))
    body = g.set(body, 'PoseChanged', either(g.get('PoseChanged'), either(g.math('FloatGreater', A=position_delta, B=1e-4), g.math('FloatGreater', A=angle_delta, B=1e-5))))
    body = g.set(body, 'ScaleChanged', either(g.get('ScaleChanged'), g.math('FloatGreater', A=g.length(g.sub(scale, g.at('FramePrevious', i, 'Scale3D'))), B=1e-5)))
    g.set(body, 'Teleport', either(g.get('Teleport'), g.math('FloatGreater', A=position_delta, B=g.math('FloatMul', A=g.math('FloatMax', A=x, B=1e-6), B=50))))
    active, invalid = g.branch(active, g.get('ValidScale'))
    skip(g, invalid)
    update = either(changed('Int', 'UpdateSerial', 'SeenUpdate'), either(changed('Int', 'ResetEpoch', 'SeenEpoch'), g.math('BoolNot', Value=g.get('Initialized'))))
    active, repeated = g.branch(active, update)
    output_pose(g, repeated)
    dt = Pin(g.unit('RigVMFunction_GetDeltaTime')+'.Result')
    proposed = g.math('FloatFloor', output='Int', Value=g.math('FloatAdd', A=g.math('FloatMul', A=g.math('FloatAdd', A=g.get('Remainder'), B=dt), B=120), B=1e-5))
    reset = either(g.math('BoolNot', Value=g.get('Initialized')), changed('Int', 'ResetEpoch', 'SeenEpoch'))
    reset = either(reset, either(g.get('ScaleChanged'), g.get('Teleport')))
    reset = either(reset, either(g.math('FloatGreater', A=dt, B=8*H+1e-6), g.math('IntGreater', A=proposed, B=8)))
    reset = either(reset, both(g.math('FloatLessEqual', A=dt, B=0), g.get('PoseChanged')))
    seed, advance = g.branch(active, reset)
    i, body, seed = loop(g, seed)
    body = g.put(body, 'Orientation', i, g.at('FrameTargets', i, 'Rotation'))
    body = g.put(body, 'Omega', i, (0, 0, 0))
    g.put(body, 'LinearVelocity', i, (0, 0, 0))
    for name, value in [('PreviousTargets', g.get('FrameTargets')), ('PreviousStepSeconds', H),
                        ('Display', g.get('Orientation')), ('Remainder', 0), ('Substeps', 0), ('PreviewSteps', 0),
                        ('ResetCount', g.math('IntAdd', A=g.get('ResetCount'), B=1))]:
        seed = g.set(seed, name, value)
    finish(g, seed)
    advance, zero = g.branch(advance, g.math('FloatGreater', A=dt, B=0))
    zero = g.set(zero, 'Substeps', 0)
    zero = g.set(zero, 'PreviewSteps', 0)
    finish(g, zero)
    for name, value in [('OldRemainder', g.get('Remainder')), ('Substeps', proposed),
                        ('Remainder', g.math('FloatAdd', A=g.get('Remainder'), B=dt)), ('StepSeconds', H)]:
        advance = g.set(advance, name, value)
    steps = g.unit('RigVMFunction_ForLoopCount', Count=g.get('Substeps'))
    g.link(advance, steps+'.ExecuteContext')
    i, body, step_end = loop(g, Pin(steps+'.ExecuteContext'))
    time = g.math('FloatMul', A=g.math('IntToFloat', Value=g.math('IntAdd', A=Pin(steps+'.Index'), B=1)), B=H)
    alpha = g.math('FloatClamp', Value=g.math('FloatDiv', A=g.math('FloatSub', A=time, B=g.get('OldRemainder')), B=dt), Minimum=0, Maximum=1)
    body = g.put(body, 'Targets', i, g.math('TransformLerp', A=g.at('FramePrevious', i), B=g.at('FrameTargets', i), T=alpha))
    if VERSION >= 9:
        g.put(body, 'ContactStep', i, g.math('TransformLerp', A=g.at('ContactPrevious', i), B=g.at('ContactCurrent', i), T=alpha))
    step(g, step_end, travel=VERSION >= 5, planes=VERSION >= 6, scaled_acceleration=VERSION >= 8, contacts=VERSION >= 9, regions=VERSION >= 10,
         travel_input=VERSION >= 11, soft=VERSION >= 11)
    end = Pin(steps+'.Completed')
    end = g.set(end, 'Remainder', g.math('FloatMax', A=0, B=g.math('FloatSub', A=g.get('Remainder'), B=g.math('FloatMul', A=g.math('IntToFloat', Value=g.get('Substeps')), B=H))))
    for name in STATE:
        end = g.set(end, 'Saved'+name, g.get(name))
    end = g.set(end, 'SavedPreviousStepSeconds', g.get('PreviousStepSeconds'))
    end = g.set(end, 'PreviewSteps', 0)
    preview, exact = g.branch(end, g.math('FloatGreater', A=g.get('Remainder'), B=1e-7))
    preview = g.set(preview, 'PreviewSteps', 1)
    preview = g.set(preview, 'StepSeconds', g.get('Remainder'))
    preview = g.set(preview, 'Targets', g.get('FrameTargets'))
    if VERSION >= 9:
        preview = g.set(preview, 'ContactStep', g.get('ContactCurrent'))
    preview = step(g, preview, False, travel=VERSION >= 5, planes=VERSION >= 6, scaled_acceleration=VERSION >= 8, contacts=VERSION >= 9, regions=VERSION >= 10,
         travel_input=VERSION >= 11, soft=VERSION >= 11)
    for end in (preview, exact):
        end = g.set(end, 'Display', g.get('Orientation'))
        for name in STATE:
            end = g.set(end, name, g.get('Saved'+name))
        end = g.set(end, 'PreviousStepSeconds', g.get('SavedPreviousStepSeconds'))
        finish(g, end)
    rig.recompile_vm()
    unreal.BlueprintEditorLibrary.compile_blueprint(rig)
    if not unreal.EditorAssetLibrary.save_loaded_asset(rig, only_if_is_dirty=False):
        raise RuntimeError('Could not save timed body rig')
    report.write_text(json.dumps(dict(package=PACKAGE, mesh=MESH, helpers=[h['bone'] for h in layout],
        nodes=len(g.controller.get_graph().get_nodes()), clock_hz=120, maximum_steps=8,
        inertial_velocity_filter_seconds=.025,
        weighted_mesh_travel_clamp=VERSION >= 5, center_halfspaces=VERSION >= 6,
        ellipsoid_support_planes=VERSION >= 9,
        independent_region_settings=VERSION >= 10,
        hierarchy_from=HIERARCHY_FROM or MESH, kept_members=sorted(kept),
        travel_input_degrees=VERSION >= 11, soft_zone_default=.25 if VERSION >= 11 else None,
        scope='Authored timed body graph; execution, timing, scale, contacts and live acceptance pending.'), indent=2)+'\n')


if __name__ == '__main__':
    main()

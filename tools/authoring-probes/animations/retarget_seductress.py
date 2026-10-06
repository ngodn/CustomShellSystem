"""Direct source-to-older-Seductress auditions, without changing released rigs."""
import hashlib
import json
import math
import os
import re
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
MODE = os.environ.get('CSS_RETARGET_MODE', 'create')
if MODE not in ('create', 'verify') or not WORK.is_relative_to(ROOT / 'CustomShellSystem/work'):
    raise ValueError('Expected isolated create or verify mode')
REPORT = WORK / (MODE + '-result.json')
if REPORT.exists():
    raise FileExistsError(REPORT)
PACKAGE = '/Game/CSS/AnimLab'
REVISION = os.environ.get('CSS_RETARGET_REVISION', 'S2')
if not re.fullmatch(r'S[1-9][0-9]{0,2}', REVISION):
    raise ValueError('Expected a numbered Seductress audition revision')
FEET_MODE = os.environ.get('CSS_RETARGET_FEET', 'none')
if FEET_MODE not in ('none', 'height'):
    raise ValueError('Expected none or height foot alignment')
SOURCE = PACKAGE + '/SK_L2_EveSource'
TARGET = '/Game/CSSAuthoring/CSS_Seductress_eins0fx_P/SK_Seductress_MidasV5'
SOURCE_RIG = PACKAGE + '/IK_L2_EveSource'
TARGET_RIG = PACKAGE + '/IK_' + REVISION + '_Seductress'
RETARGET = PACKAGE + '/RT_' + REVISION + '_Seductress'
CLIPS = [('BlackHole', 'S02A000'), ('EveCatwalk', 'S03A000'), ('ComeCloser', 'S04A000')]
chains = json.loads((WORK.parent / 'retarget-l2/chains.json').read_text())


def disk(path):
    return CONTENT / (path.removeprefix('/Game/') + '.uasset')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise ValueError('Missing asset: ' + path)
    return asset


def create(path, kind, factory):
    if disk(path).exists() or unreal.EditorAssetLibrary.does_asset_exist(path):
        raise FileExistsError(path)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        path.rsplit('/', 1)[1], path.rsplit('/', 1)[0], kind, factory)
    if not asset:
        raise ValueError('Cannot create: ' + path)
    return asset


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise ValueError('Cannot save: ' + asset.get_path_name())


def bind_world(mesh):
    bones = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))
    result = []
    for index, bone in enumerate(bones):
        if not -1 <= bone['parent'] < index:
            raise ValueError('Expected parent-first bind pose')
        local = unreal.Transform(
            location=unreal.Vector(*bone['translation']),
            rotation=unreal.Quat(*bone['rotation']).rotator(),
            scale=unreal.Vector(*bone['scale']))
        world = local if bone['parent'] < 0 else local.multiply(result[bone['parent']][1])
        result.append((bone['name'], world))
    return dict(result)


source, target, source_rig = (load(path) for path in (SOURCE, TARGET, SOURCE_RIG))
protected_paths = [SOURCE, TARGET, SOURCE_RIG,
                   source.get_editor_property('skeleton').get_path_name().split('.')[0],
                   target.get_editor_property('skeleton').get_path_name().split('.')[0]]
protected_paths += [PACKAGE + '/AN_L2_' + suffix for _, suffix in CLIPS]
protected = {str(disk(path)): digest(disk(path)) for path in protected_paths}
if MODE == 'create':
    rig = create(TARGET_RIG, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    rig_controller = unreal.IKRigController.get_controller(rig)
    if not rig_controller.set_skeletal_mesh(target) or not rig_controller.set_retarget_root('pelvis'):
        raise ValueError('Cannot set target rig mesh/root')
    for name, _, _, start, end in chains:
        if str(rig_controller.add_retarget_chain(name, start, end, 'None')) != name:
            raise ValueError('Cannot add target chain: ' + name)
    if FEET_MODE == 'height':
        for side_name, suffix in (('L', 'l'), ('R', 'r')):
            goal = 'Foot' + side_name
            if str(rig_controller.add_new_goal(goal, 'foot_' + suffix)) != goal:
                raise ValueError('Cannot add foot goal')
            solver = rig_controller.add_solver('/Script/IKRig.IKRigLimbSolver')
            if (solver < 0 or not rig_controller.set_start_bone('thigh_' + suffix, solver) or
                    not rig_controller.connect_goal_to_solver(goal, solver) or
                    not rig_controller.set_retarget_chain_goal('Leg' + side_name, goal)):
                raise ValueError('Cannot bind foot solver')
    save(rig)
    retarget = create(RETARGET, unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(retarget)
    side = unreal.RetargetSourceOrTarget
    controller.set_ik_rig(side.SOURCE, source_rig)
    controller.set_ik_rig(side.TARGET, rig)
    controller.set_preview_mesh(side.SOURCE, source)
    controller.set_preview_mesh(side.TARGET, target)
    controller.add_default_ops()
    for name, *_ in chains:
        if not controller.set_source_chain(name, name):
            raise ValueError('Cannot map chain: ' + name)
    controller.auto_align_all_bones(side.TARGET)
    if FEET_MODE == 'height':
        source_world, target_world = bind_world(source), bind_world(target)
        for index in (1, 3):
            op = controller.get_op_controller(index)
            settings = op.get_settings()
            settings.set_editor_property('ik_rig_asset', rig)
            op.set_settings(settings)
        op = controller.get_op_controller(2)
        settings = op.get_settings()
        foot_chains = list(settings.get_editor_property('chains_to_retarget'))
        if {str(c.get_editor_property('target_chain_name')) for c in foot_chains} != {'LegL', 'LegR'}:
            raise ValueError('Unexpected goal chains')
        offsets = {}
        for chain in foot_chains:
            label = str(chain.get_editor_property('target_chain_name'))
            suffix = label[-1].lower()
            offset = (target_world['foot_' + suffix].translation.z -
                      source_world['Bip001-' + label[-1] + '-Foot'].translation.z)
            if not controller.set_source_chain(label, label, controller.get_op_name(3)):
                raise ValueError('Cannot map foot chain')
            chain.set_editor_property('enable_ik', True)
            chain.set_editor_property('blend_to_source', 1.0)
            chain.set_editor_property('blend_to_source_weights', unreal.Vector(0, 0, 1))
            chain.set_editor_property('static_offset', unreal.Vector(0, 0, offset))
            offsets[label] = offset
        settings.set_editor_property('chains_to_retarget', foot_chains)
        op.set_settings(settings)
        (WORK / 'foot-height-offsets.json').write_text(json.dumps(offsets, indent=2) + '\n')
    save(retarget)
else:
    if json.loads((WORK / 'create-result.json').read_text())['feet_mode'] != FEET_MODE:
        raise ValueError('Verification foot mode differs from creation')
    retarget = load(RETARGET)
    protected.update({str(disk(p)): digest(disk(p)) for p in (RETARGET, TARGET_RIG)})

options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh', target)
options.set_editor_property('should_retarget', True)


def snapshot(animation, frame):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(animation, frame, options)
    if not unreal.AnimPoseExtensions.is_valid(pose):
        raise ValueError('Invalid evaluated pose')
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
    transforms = []
    for name in names:
        t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
        row = dict(Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                   Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                   Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z))))
        if not all(math.isfinite(v) for values in row.values() for v in values.values()):
            raise ValueError('Non-finite transform: ' + name)
        transforms.append(row)
    return dict(pose=dict(Snapshot=dict(bIsValid=True, SkeletalMeshName=target.get_name(),
                                        BoneNames=names, LocalTransforms=transforms)))


results = []
for name, suffix in CLIPS:
    animation = load(PACKAGE + '/AN_L2_' + suffix)
    output = PACKAGE + '/RT_' + REVISION + '_' + name
    path = output if MODE == 'create' else output + 'Check'
    candidate = unreal.CSSAnimationLibrary.retarget_clip(source, target, animation, retarget, path)
    if not candidate or candidate.get_editor_property('skeleton') != target.get_editor_property('skeleton'):
        raise ValueError('Retarget failed: ' + name)
    keys = unreal.AnimationLibrary.get_num_keys(animation)
    duration = unreal.AnimationLibrary.get_sequence_length(animation)
    hz = (keys - 1) / duration
    if (unreal.AnimationLibrary.get_num_keys(candidate) != keys or
            abs(unreal.AnimationLibrary.get_sequence_length(candidate) -
                unreal.AnimationLibrary.get_sequence_length(animation)) > 1e-6):
        raise ValueError('Retarget changed timing')
    motion_file = WORK / (name + '-motion.json')
    if MODE == 'create':
        motion = dict(fps=hz, frames=[snapshot(candidate, f) for f in range(keys)])
        motion_file.write_text(json.dumps(motion) + '\n')
        save(candidate)
        result = dict(name=name, output=output, keys=keys, hz=hz, sha256=digest(disk(output)))
    else:
        protected[str(disk(output))] = digest(disk(output))
        motion = json.loads(motion_file.read_text())
        if len(motion['frames']) != keys or abs(motion['fps'] - hz) > 1e-6:
            raise ValueError('Saved timing does not match source')
        worst_position = worst_rotation = worst_scale = 0.0
        for clip in (load(output), candidate):
            for frame, expected in enumerate(motion['frames']):
                actual = snapshot(clip, frame)['pose']['Snapshot']
                expected = expected['pose']['Snapshot']
                if actual['BoneNames'] != expected['BoneNames']:
                    raise ValueError('Saved/repeated pose has different bones')
                for a, b in zip(actual['LocalTransforms'], expected['LocalTransforms']):
                    worst_position = max(worst_position, math.dist(a['Translation'].values(), b['Translation'].values()))
                    q, r = list(a['Rotation'].values()), list(b['Rotation'].values())
                    worst_rotation = max(worst_rotation, min(math.dist(q, r), math.dist(q, [-x for x in r])))
                    worst_scale = max(worst_scale, math.dist(a['Scale3D'].values(), b['Scale3D'].values()))
        if worst_position > .001 or worst_rotation > .0001 or worst_scale > .0001:
            raise ValueError('Saved/repeated conversion differs: ' + name)
        result = dict(name=name, keys=keys, hz=hz, max_position_cm=worst_position,
                      max_quaternion_distance=worst_rotation, max_scale_distance=worst_scale)
    results.append(result)
    REPORT.write_text(json.dumps(dict(complete=False, results=results), indent=2) + '\n')
if any(digest(Path(p)) != h for p, h in protected.items()):
    raise ValueError('Protected input changed')
REPORT.write_text(json.dumps(dict(complete=True, feet_mode=FEET_MODE, results=results, protected=protected,
    scope='Raw retarget auditions and saved/repeated readback only. '
          'Skinned contact, physics adapter evaluation, cook and live playback remain pending.'), indent=2) + '\n')

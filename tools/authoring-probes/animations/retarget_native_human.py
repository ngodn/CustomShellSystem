"""UE 5.6.1 native-human audition using the decoded game reference."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
REPO = ROOT / 'CustomShellSystem'
WORK = Path(os.environ['CSS_NATIVE_RETARGET_WORK']).resolve()
assert WORK.resolve().is_relative_to((REPO / 'work/cine-animation-port').resolve())
assert not (WORK / 'result.json').exists()
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
PACKAGE = '/Game/CSS/AnimLab'
SOURCE = PACKAGE + '/SK_L2_EveSource'
TARGET = '/Game/CSSAuthoring/DiagnosticReferences/SK_GameHuman1199_V1'
SKELETON = '/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameHuman1199_V1'
CLIP = PACKAGE + '/AN_L2_S02A000'
allow_reference_scale = os.environ.get('CSS_NATIVE_REFERENCE_SCALE') == '1'


def write(name, value):
    (WORK / name).write_text(json.dumps(value, indent=2) + '\n')


def file_for(package):
    return CONTENT / (package.removeprefix('/Game/') + '.uasset')


def compare(actual, expected):
    assert len(actual) == len(expected)
    errors = dict(translation=0.0, rotation=0.0, scale=0.0)
    for a, b in zip(actual, expected, strict=True):
        assert (a['name'].lower(), a['parent']) == (b['name'].lower(), b['parent'])
        for key in errors:
            delta = math.dist(a[key], b[key])
            if key == 'rotation':
                delta = min(delta, math.dist(a[key], [-v for v in b[key]]))
            errors[key] = max(errors[key], delta)
    assert max(errors.values()) < 0.00001, errors
    return errors


paths = [file_for(p) for p in (SOURCE, TARGET, SKELETON, CLIP,
         '/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton')]
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
write('protected.json', hashes)
assets = json.loads((REPO / 'work/cine-animation-port/native-human-compatibility/assets.json').read_text())
decoded = next(e for e in assets['/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton']
               if e['Type'] == 'Skeleton')
ref = decoded['ReferenceSkeleton']
expected = [dict(name=b['Name'], parent=b['ParentIndex'],
                 translation=[t['Translation'][k] for k in 'XYZ'],
                 rotation=[t['Rotation'][k] for k in 'XYZW'],
                 scale=[t['Scale3D'][k] for k in 'XYZ'])
            for b, t in zip(ref['FinalRefBoneInfo'], ref['FinalRefBonePose'], strict=True)]
source, target, skeleton, animation = [unreal.load_asset(p) for p in (SOURCE, TARGET, SKELETON, CLIP)]
assert source and target and skeleton and animation
assert target.get_editor_property('skeleton') == skeleton
mesh_bind = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target))
skeleton_bind = json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(skeleton))
assert len(mesh_bind) == len(skeleton_bind) == len(expected) == 1199
reference = dict(mesh=compare(mesh_bind, expected), skeleton=compare(skeleton_bind, expected))
mode_map = {'Animation': unreal.BoneTranslationRetargetingMode.ANIMATION,
            'AnimationScaled': unreal.BoneTranslationRetargetingMode.ANIMATION_SCALED,
            'Skeleton': unreal.BoneTranslationRetargetingMode.SKELETON}
assert all(a['translation_mode'] == mode_map[b['TranslationRetargetingMode'].split('::')[-1]].value
           for a, b in zip(skeleton_bind, decoded['Properties']['BoneTree'], strict=True))
virtual = json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(skeleton))
write('virtual-bones.json', virtual)
assert len(virtual) == 9
assert virtual == decoded['Properties']['VirtualBones']
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(target) == 9
write('reference.json', dict(errors=reference, raw_bones=1199, virtual_bones=virtual,
      mirrored_bones=[b for b in expected if min(b['scale']) < 0]))

tools = unreal.AssetToolsHelpers.get_asset_tools()


def create(name, kind, factory):
    assert not unreal.EditorAssetLibrary.does_asset_exist(PACKAGE + '/' + name)
    asset = tools.create_asset(name, PACKAGE, kind, factory)
    assert asset
    return asset


chains = json.loads((REPO / 'work/cine-animation-port/retarget-l2/chains.json').read_text())
rig = create('IK_NH1_Target', unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
controller = unreal.IKRigController.get_controller(rig)
assert controller.set_skeletal_mesh(target)
assert controller.set_retarget_root('pelvis')
for name, _, _, start, end in chains:
    assert str(controller.add_retarget_chain(name, start, end, 'None')) == name
source_rig = unreal.load_asset(PACKAGE + '/IK_L2_EveSource')
assert source_rig
retarget = create('RT_NH1_Target', unreal.IKRetargeter, unreal.IKRetargetFactory())
controller = unreal.IKRetargeterController.get_controller(retarget)
mode = unreal.RetargetSourceOrTarget
controller.set_ik_rig(mode.SOURCE, source_rig)
controller.set_ik_rig(mode.TARGET, rig)
controller.set_preview_mesh(mode.SOURCE, source)
controller.set_preview_mesh(mode.TARGET, target)
controller.add_default_ops()
for chain in chains:
    assert controller.set_source_chain(chain[0], chain[0])
controller.auto_align_all_bones(mode.TARGET)
args = [source, target, animation, retarget, PACKAGE + '/AN_NH1_BlackHole', True]
if allow_reference_scale:
    args.append(True)
candidate = unreal.CSSAnimationLibrary.retarget_clip(*args)
assert candidate
affected = set()
for bone in expected:
    parent = expected[bone['parent']]['name'] if bone['parent'] >= 0 else None
    if math.dist(bone['scale'], [1, 1, 1]) > .001 or parent in affected:
        affected.add(bone['name'])
assert len(affected) >= 6
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh', target)
options.set_editor_property('should_retarget', False)
source_data = json.loads((REPO / 'work/cine-animation-port/retarget-l2/S02A000.json').read_text())
baseline = None
if allow_reference_scale:
    baseline = unreal.CSSAnimationLibrary.retarget_clip(
        source, target, animation, retarget, PACKAGE + '/AN_NH1_Control', True)
    assert baseline
errors = dict(translation=0.0, rotation=0.0, scale=0.0)
unchanged_error = 0.0
for frame in range(source_data['frames']):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(candidate, frame/source_data['fps'], options)
    assert unreal.AnimPoseExtensions.is_valid(pose)
    control = unreal.AnimPoseExtensions.get_anim_pose_at_time(baseline, frame/source_data['fps'], options) if baseline else None
    for bone in expected:
        name = bone['name']
        t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
        values = dict(translation=[t.translation.x, t.translation.y, t.translation.z],
                      rotation=[t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w],
                      scale=[t.scale3d.x, t.scale3d.y, t.scale3d.z])
        assert all(math.isfinite(v) for row in values.values() for v in row)
        if name in affected:
            for key in errors:
                delta = math.dist(values[key], bone[key])
                if key == 'rotation':
                    delta = min(delta, math.dist(values[key], [-v for v in bone[key]]))
                errors[key] = max(errors[key], delta)
        elif control:
            c = unreal.AnimPoseExtensions.get_bone_pose(control, name, unreal.AnimPoseSpaces.LOCAL)
            difference = max(math.dist(values['translation'], [c.translation.x, c.translation.y, c.translation.z]),
                             math.dist(values['rotation'], [c.rotation.x, c.rotation.y, c.rotation.z, c.rotation.w]),
                             math.dist(values['scale'], [c.scale3d.x, c.scale3d.y, c.scale3d.z]))
            unchanged_error = max(unchanged_error, difference)
    if frame % 100 == 0:
        write('progress.json', dict(frame=frame, frames=source_data['frames']))
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in hashes.items())
report = dict(retarget_created=True, reference=reference, reference_scale_enabled=allow_reference_scale,
              frames=source_data['frames'], fps=source_data['fps'], affected_bones=sorted(affected),
              reference_errors=errors, unaffected_max_error=unchanged_error, protected=hashes,
              scope='Full raw clip on the diagnostic native reference. Not gameplay or skinned-mesh acceptance.')
write('result.json', report)
if allow_reference_scale:
    assert max(errors.values()) < .00001, errors
    assert unchanged_error == 0, unchanged_error
    assert unreal.EditorAssetLibrary.save_loaded_asset(candidate, False)
else:
    assert errors['scale'] > 1, 'Expected the engine to strip mirrored reference scales'

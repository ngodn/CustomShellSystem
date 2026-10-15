"""Configure and evaluate the isolated NPC imports with the retained native H2 clip."""
import hashlib
import json
import math
from pathlib import Path

import unreal

CSS = Path(__file__).resolve().parents[2]
OUT = CSS / 'work/npc-appearances'
REFERENCE = CSS / 'work/grip-grounding-v1'
human = unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameHumanMore1275_V1')
if not human:
    raise RuntimeError('The retained player reference Skeleton is missing')
source_file = REFERENCE / 'active-h2-absolute-tracks-v1/absolute-tracks.json'
tracks = json.loads(source_file.read_text())
source_names = {b['name'] for b in json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(human))}
omitted_virtual_tracks = []
factory = unreal.AnimSequenceFactory()
factory.set_editor_property('target_skeleton', human)
animation = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'AN_NPC_H2', '/Game/CSSAuthoring/TransientProbes', unreal.AnimSequence, factory)
if not animation:
    raise RuntimeError('Cannot create the temporary source animation')
animation.set_editor_property('retarget_source', tracks['retargetSource'])
controller = animation.get_editor_property('controller')
controller.open_bracket('Native H2 samples for NPC binding', False)
try:
    controller.set_frame_rate(unreal.FrameRate(30, 1), False)
    controller.set_number_of_frames(unreal.FrameNumber(len(tracks['frames']) - 1), False)
    for i, bone in enumerate(tracks['frames'][0]['BoneNames']):
        if bone not in source_names and bone.startswith('VB '):
            omitted_virtual_tracks.append(bone)
            continue
        values = [frame['LocalTransforms'][i] for frame in tracks['frames']]
        if not controller.add_bone_curve(bone, False):
            raise RuntimeError('Cannot add source track: ' + bone)
        if not controller.set_bone_track_keys(
                bone, [unreal.Vector(*[t['Translation'][c] for c in 'XYZ']) for t in values],
                [unreal.Quat(*[t['Rotation'][c] for c in 'XYZW']) for t in values],
                [unreal.Vector(*[t['Scale3D'][c] for c in 'XYZ']) for t in values], False):
            raise RuntimeError('Cannot import source track: ' + bone)
finally:
    controller.close_bracket(False)

report = {'source_clip_sha256': hashlib.sha256(source_file.read_bytes()).hexdigest(),
          'omitted_virtual_tracks': omitted_virtual_tracks,
          'scope': 'Native sampled H2 animation on new NPC Skeletons. Not gameplay or physics acceptance.',
          'characters': {}}
for name in ('Zhirelle', 'Merrick'):
    prefix = '/Game/CSS/eins0fx/NPC/' + name
    mesh = unreal.load_asset(prefix + '/SK_Body')
    skeleton = unreal.load_asset(prefix + '/SKEL_Body')
    if not mesh or not skeleton or mesh.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('Missing imported NPC binding: ' + name)
    expected = json.loads((OUT / (name.lower() + '.mesh.json')).read_text())['bones']
    before = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))
    if [(b['name'], b['parent']) for b in before] != [(b['name'], b['parent']) for b in expected]:
        raise RuntimeError('Imported bone mapping differs: ' + name)
    for actual, source in zip(before, expected, strict=True):
        for key in ('translation', 'rotation', 'scale'):
            error = math.dist(actual[key], source[key])
            if key == 'rotation':
                error = min(error, math.dist(actual[key], [-x for x in source[key]]))
            if error > .001:
                raise RuntimeError('Imported bind pose differs: ' + actual['name'])
    skeleton.set_editor_property('compatible_skeletons', [human])
    adjusted = unreal.CSSNpcLibrary.configure_animation_reference(skeleton, human)
    if adjusted < 60:
        raise RuntimeError('Cannot configure the NPC animation reference')
    if json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh)) != before:
        raise RuntimeError('Animation reference changed the mesh bind')
    names = [b['name'] for b in expected]
    if not unreal.CSSRetargetLibrary.set_translation_retargeting(
            skeleton, names, unreal.BoneTranslationRetargetingMode.SKELETON):
        raise RuntimeError('Cannot configure NPC translation modes')
    if not unreal.CSSRetargetLibrary.set_translation_retargeting(
            skeleton, ['root', 'ik_foot_root', 'ik_foot_l', 'ik_foot_r', 'ik_hand_root',
                       'ik_hand_gun', 'ik_hand_l', 'ik_hand_r'],
            unreal.BoneTranslationRetargetingMode.ANIMATION):
        raise RuntimeError('Cannot configure NPC root and IK translation')
    if not unreal.CSSRetargetLibrary.set_translation_retargeting(
            skeleton, ['pelvis'], unreal.BoneTranslationRetargetingMode.ANIMATION_SCALED):
        raise RuntimeError('Cannot configure NPC pelvis translation')
    (OUT / (name.lower() + '-animation-reference.json')).write_text(
        unreal.CSSRetargetLibrary.inspect_skeleton(skeleton) + '\n')
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('should_retarget', True)
    options.set_editor_property('optional_skeletal_mesh', mesh)
    samples = []
    for i in range(len(tracks['frames'])):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(animation, i / 30, options)
        pose_names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
        if pose_names != names:
            raise RuntimeError('Native retargeting returned the wrong bones: ' + name)
        bones = []
        for bone in names:
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL)
            p, q, s = t.translation, t.rotation, t.scale3d
            value = dict(name=bone, translation=[p.x, p.y, p.z],
                         rotation=[q.x, q.y, q.z, q.w], scale=[s.x, s.y, s.z])
            if not all(math.isfinite(v) for key in ('translation', 'rotation', 'scale') for v in value[key]):
                raise RuntimeError('Nonfinite retargeted pose: ' + bone)
            bones.append(value)
        samples.append(bones)
    (OUT / (name.lower() + '-poses.json')).write_text(json.dumps(samples) + '\n')
    # The reference belongs to the offline fixture and must not enter a cook.
    skeleton.set_editor_property('compatible_skeletons', [])
    if not unreal.EditorAssetLibrary.save_loaded_asset(skeleton, False):
        raise RuntimeError('Cannot save NPC Skeleton: ' + name)
    report['characters'][name] = dict(bones=len(names), samples=len(samples), adjusted_reference_rotations=adjusted,
                                      skeleton=skeleton.get_path_name(), mesh=mesh.get_path_name())
(OUT / 'configured.json').write_text(json.dumps(report, indent=2) + '\n')
unreal.log('CSS_NPC_BINDINGS_CONFIGURED')

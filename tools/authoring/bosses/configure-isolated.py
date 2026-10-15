"""Configure and evaluate the isolated NPC imports with the retained native H2 clip."""
import hashlib
import json
import math
from pathlib import Path

import unreal

CSS = Path('/home/eins0fx/development/mods/msII/CustomShellSystem')
OUT = CSS / 'work/npc-appearances/bosses'
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

plans = json.loads((OUT / 'isolated-plan.json').read_text())
report = {'scope': 'Isolated skeleton translation retargeting; native mesh payloads remain separate.', 'characters': {}}
for plan in plans:
    skeleton = unreal.load_asset(plan['skeleton'])
    mesh = unreal.load_asset('/Game/CSSAuthoring/BossRigSeeds/SK_' + plan['id'])
    assert skeleton and mesh
    before = json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(skeleton))
    assert len(before) == len(plan['bones'])
    for actual, expected in zip(before, plan['bones']):
        assert actual['name'].lower() == expected['name'].lower() and actual['parent'] == expected['parent']
        for field in ('translation','rotation','scale'):
            error=math.dist(actual[field],expected[field])
            if field=='rotation':error=min(error, math.dist(actual[field],[-v for v in expected[field]]))
            assert error<.001, (actual['name'],field,error)
    names = [b['name'] for b in before]
    assert unreal.CSSRetargetLibrary.set_translation_retargeting(skeleton,names,unreal.BoneTranslationRetargetingMode.SKELETON)
    root_ik=[n for n in names if n.lower()=='root' or n.lower().startswith('ik_')]
    assert unreal.CSSRetargetLibrary.set_translation_retargeting(skeleton,root_ik,unreal.BoneTranslationRetargetingMode.ANIMATION)
    assert unreal.CSSRetargetLibrary.set_translation_retargeting(skeleton,['pelvis'],unreal.BoneTranslationRetargetingMode.ANIMATION_SCALED)
    assert unreal.CSSRetargetLibrary.restore_auxiliary_bones_and_sockets(skeleton,json.dumps({'bones':[],'sockets':plan['sockets']}))
    skeleton.set_editor_property('compatible_skeletons',[human])
    options=unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.RAW)
    options.set_editor_property('should_retarget',True)
    options.set_editor_property('optional_skeletal_mesh',mesh)
    frames=[]
    for i in sorted({0,len(tracks['frames'])//4,len(tracks['frames'])//2,3*len(tracks['frames'])//4,len(tracks['frames'])-1}):
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(animation,i/30,options)
        order=[str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
        assert len(order)==len(names)
        values=[]
        for name in order:
            t=unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.LOCAL)
            values.append(dict(Translation=dict(zip('XYZ',[t.translation.x,t.translation.y,t.translation.z])),Rotation=dict(zip('XYZW',[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w])),Scale3D=dict(zip('XYZ',[t.scale3d.x,t.scale3d.y,t.scale3d.z]))))
        frames.append({'source_frame':i,'pose':{'Snapshot':{'bIsValid':True,'BoneNames':order,'LocalTransforms':values}}})
    (OUT/(plan['id']+'-corrected-motion.json')).write_text(json.dumps({'fps':30,'frames':frames}))
    skeleton.set_editor_property('compatible_skeletons',[])
    if plan['virtual']:
        # The helper only edits authoring references. Restore there, then copy the
        # finished Skeleton to its isolated game path using the editor's API.
        temporary='/Game/CSSAuthoring/BossRigSeeds/SKEL_' + plan['id']
        assert not unreal.EditorAssetLibrary.does_asset_exist(temporary)
        copy=unreal.EditorAssetLibrary.duplicate_loaded_asset(skeleton,temporary)
        assert copy and unreal.CSSRetargetLibrary.restore_virtual_bones(copy,json.dumps(plan['virtual']))
        assert unreal.EditorAssetLibrary.save_loaded_asset(copy,False)
        assert unreal.EditorAssetLibrary.delete_asset(plan['skeleton'])
        skeleton=unreal.EditorAssetLibrary.duplicate_loaded_asset(copy,plan['skeleton'])
        assert skeleton
    assert unreal.EditorAssetLibrary.save_loaded_asset(skeleton,False)
    report['characters'][plan['id']]={'skeleton':plan['skeleton'],'bones':len(before),'sockets':len(plan['sockets']),'virtual_bones':len(plan['virtual']),'samples':len(frames)}
(OUT/'isolated-configured.json').write_text(json.dumps(report,indent=2))
unreal.log('CSS_BOSS_SKELETONS_CONFIGURED')

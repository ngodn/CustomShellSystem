import os
"""UE5.6.1 isolated game-derived Skeleton foundation comparison. No production save."""
import json,math,hashlib
from pathlib import Path
import unreal
OUT=Path(os.environ['CSS_FOUNDATION_AUDIT_DIR']).resolve();ROOT=OUT.parents[3]
assert OUT.parent == Path(__file__).resolve().parents[4]/'CustomShellSystem/work/grip-grounding-v1';BASE=OUT.parent/'game-human-reference-editor-v1'
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
source_export=next(e for e in json.loads((OUT.parent/'live-source-skeleton-v1/decoded/SKEL_Human_Skeleton.json').read_text()) if e['Type']=='Skeleton')
source_bones=json.loads((BASE/'GameHuman1199.mesh.json').read_text())['bones']
more_bones=json.loads((BASE/'MoreBeaute258.mesh.json').read_text())['bones']
more_index={b['name'].lower():i for i,b in enumerate(more_bones)}
diagnostic_source_bones=json.loads((OUT/'source.mesh.json').read_text())['bones']
source_index={b['name'].lower():i for i,b in enumerate(source_bones)}
human=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameHumanMore1275_V1')
control=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_MoreBeaute258_V1')
target=unreal.load_asset('/Game/CSSAuthoring/CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43')
shared=unreal.load_asset('/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base')
assert human and control and target and shared
production=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSSAuthoring'
paths=[production/'CSS_SeduXtress_eins0fx_P/SK_SeduXtress_BodyRigV42.uasset',production/'Shared/Skeletons/SKEL_CSS_Base.uasset',production/'CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43.uasset']
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}

def check(actual,expected):
 assert len(actual)==len(expected)
 errors={k:0.0 for k in ('translation','rotation','scale')}
 for a,b in zip(actual,expected,strict=True):
  assert (a['name'].lower(),a['parent'])==(b['name'].lower(),b['parent'])
  for key in errors:
   error=math.dist(a[key],b[key])
   if key=='rotation':error=min(error,math.dist(a[key],[-v for v in b[key]]))
   errors[key]=max(errors[key],error)
 assert max(errors.values())<.001,errors
 return errors
reference_error=check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(human)),diagnostic_source_bones)
check(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(control)),more_bones)
mode_map={'Animation':unreal.BoneTranslationRetargetingMode.ANIMATION,'AnimationScaled':unreal.BoneTranslationRetargetingMode.ANIMATION_SCALED,'Skeleton':unreal.BoneTranslationRetargetingMode.SKELETON}
for name,mode in mode_map.items():
 names=[b['name'] for b,t in zip(source_bones,source_export['Properties']['BoneTree'],strict=True) if t['TranslationRetargetingMode'].split('::')[-1]==name]
 assert unreal.CSSRetargetLibrary.set_translation_retargeting(human,names,mode)
assert unreal.CSSRetargetLibrary.set_translation_retargeting(human,[b['name'] for b in diagnostic_source_bones[1199:]],unreal.BoneTranslationRetargetingMode.SKELETON)
assert unreal.CSSRetargetLibrary.restore_virtual_bones(human,json.dumps(source_export['Properties']['VirtualBones']))
assert len(json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(human)))==9
configured=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(human))
assert all(a['translation_mode']==mode_map[b['TranslationRetargetingMode'].split('::')[-1]].value for a,b in zip(configured[:1199],source_export['Properties']['BoneTree'],strict=True))
# Save only the diagnostic source Skeleton. Production compatibility changes are unsaved.
# Keep diagnostic runtime configuration unsaved.
assert unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(control,human)
check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(human)),diagnostic_source_bones)
shared.add_compatible_skeleton_soft(human)
candidate_skeleton=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SKEL_CSSGameRotations_V1')
assert candidate_skeleton
candidate_expected=json.loads((OUT.parent/'game-reference-rotation-candidate-v1/candidate.mesh.json').read_text())['bones']
check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(candidate_skeleton)),candidate_expected)
shared_modes=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(shared))
for mode in set(b['translation_mode'] for b in shared_modes):
 names=[b['name'] for b in shared_modes if b['translation_mode']==mode]
 enum=next(v for v in mode_map.values() if v.value==mode)
 assert unreal.CSSRetargetLibrary.set_translation_retargeting(candidate_skeleton,names,enum)
assert unreal.CSSRetargetLibrary.restore_virtual_bones(candidate_skeleton,unreal.CSSRetargetLibrary.inspect_virtual_bones(shared))
assert len(json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(candidate_skeleton)))==9
candidate_skeleton.add_compatible_skeleton_soft(human)
candidate=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SK_V43GameRotations_V1','/Game/CSSAuthoring/DiagnosticReferences',target)
assert candidate
original_bind=json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target))
check(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(candidate)),original_bind)
assert unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(candidate,candidate_skeleton)
check(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(candidate)),original_bind)
check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(candidate_skeleton)),candidate_expected)

# Keep every graph probe on diagnostic copies and rebuild their virtual suffix.
baseline=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SK_V43BaselineIK_V1','/Game/CSSAuthoring/DiagnosticReferences',target)
assert baseline
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(control)==9
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(candidate)==9
check(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(control)),more_bones)
check(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(candidate)),original_bind)
# Full game reference plus existing CSS-only bones, with unchanged target mesh bind.
foundation_skeleton=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameFoundation1396_V1')
assert foundation_skeleton
foundation_expected=json.loads((OUT/'foundation.mesh.json').read_text())['bones']
check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(foundation_skeleton)),foundation_expected)
css_mode_by_name={b['name'].lower():b['translation_mode'] for b in shared_modes}
foundation_modes={b['name']:int(mode_map[t['TranslationRetargetingMode'].split('::')[-1]].value) for b,t in zip(source_bones,source_export['Properties']['BoneTree'],strict=True)}
foundation_modes.update({b['name']:css_mode_by_name[b['name'].lower()] for b in foundation_expected[1199:]})
for mode in set(foundation_modes.values()):
 enum=next(v for v in mode_map.values() if v.value==mode)
 assert unreal.CSSRetargetLibrary.set_translation_retargeting(foundation_skeleton,[n for n,m in foundation_modes.items() if m==mode],enum)
assert unreal.CSSRetargetLibrary.restore_virtual_bones(foundation_skeleton,json.dumps(source_export['Properties']['VirtualBones']))
foundation_skeleton.add_compatible_skeleton_soft(human)
foundation=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SK_V43GameFoundation_V1','/Game/CSSAuthoring/DiagnosticReferences',target)
assert foundation and unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(foundation,foundation_skeleton)
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(foundation)==9
check(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(foundation)),original_bind)
check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(foundation_skeleton)),foundation_expected)
mixed_skeleton=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SKEL_GameFoundationCSSModes_V1','/Game/CSSAuthoring/DiagnosticReferences',foundation_skeleton)
assert mixed_skeleton
for mode in set(css_mode_by_name.values()):
 enum=next(v for v in mode_map.values() if v.value==mode)
 assert unreal.CSSRetargetLibrary.set_translation_retargeting(mixed_skeleton,[b['name'] for b in shared_modes if b['translation_mode']==mode],enum)
mixed_skeleton.add_compatible_skeleton_soft(human)
mixed=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SK_V43GameFoundationCSSModes_V1','/Game/CSSAuthoring/DiagnosticReferences',target)
assert mixed and unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mixed,mixed_skeleton)
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(mixed)==9
check(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mixed)),original_bind)
check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(mixed_skeleton)),foundation_expected)
references={}
for label,skel in [('source',human),('target',shared),('candidate',candidate_skeleton),('foundation',foundation_skeleton),('foundation_css_modes',mixed_skeleton)]:
 flag=skel.get_editor_property('use_retarget_modes_from_compatible_skeleton')
 assert flag is False,(label,flag)
 refs=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(skel))
 references[label]=dict(asset=skel.get_path_name(),use_source_retarget_modes=flag,bones=refs,virtual_bones=json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(skel)))
(OUT/'configured-references.json').write_text(json.dumps(references,indent=2)+'\n')

# Earlier mesh binding, same fitted Eve geometry, on the corrected reference.
v42_source=unreal.load_asset('/Game/CSSAuthoring/CSS_SeduXtress_eins0fx_P/SK_SeduXtress_BodyRigV42')
assert v42_source
v42_bind=json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(v42_source))
v42=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SK_V42GameFoundationCSSModes_V1','/Game/CSSAuthoring/DiagnosticReferences',v42_source)
assert v42 and unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(v42,mixed_skeleton)
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(v42)==9
check(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(v42)),v42_bind)
(OUT/'v42-bind.json').write_text(json.dumps(v42_bind,indent=2)+'\n')

tracks=json.loads((OUT.parent/'active-h2-absolute-tracks-v1/absolute-tracks.json').read_text())
assert len(tracks['referencePose'])==1208
extra_reference=[dict(Translation=dict(zip('XYZ',b['translation'])),Rotation=dict(zip('XYZW',b['rotation'])),Scale3D=dict(zip('XYZ',b['scale']))) for b in diagnostic_source_bones[1199:]]
authored_reference=tracks['referencePose'][:1199]+extra_reference+tracks['referencePose'][1199:]
assert len(authored_reference)==1284
assert unreal.CSSRetargetLibrary.set_diagnostic_retarget_source(human,tracks['retargetSource'],json.dumps(authored_reference))
check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(human)),diagnostic_source_bones)
node=next(e for e in json.loads((OUT.parent/'player-hand-graph/decoded/ABP_Player.json').read_text()) if e.get('Name')=='Default__ABP_Player_C')['Properties']['AnimGraphNode_TwoBoneIK_1']
assert node['IKBone']['BoneName']=='hand_l'
graph=next(e for e in json.loads((OUT.parent/'player-hand-graph/decoded/ABP_Player.json').read_text()) if e.get('Name')=='Default__ABP_Player_C')['Properties']
copy_nodes=[graph['AnimGraphNode_CopyBone_1'],graph['AnimGraphNode_CopyBone']]
assert [n['TargetBone']['BoneName'] for n in copy_nodes]==['VB hand_r','VB hand_l']
assert [n['ComponentPose']['LinkID'] for n in copy_nodes]==[13,10] and node['ComponentPose']['LinkID']==11

factory=unreal.AnimSequenceFactory();factory.set_editor_property('target_skeleton',human);factory.set_editor_property('preview_skeletal_mesh',control)
animation=unreal.AssetToolsHelpers.get_asset_tools().create_asset('AN_FoundationV42H2Samples_V2','/Game/CSSAuthoring/TransientProbes',unreal.AnimSequence,factory);assert animation
animation.set_editor_property('retarget_source',tracks['retargetSource'])
controller=animation.get_editor_property('controller');controller.open_bracket('Original H2 sampled tracks including virtual bones',False)
try:
 controller.set_frame_rate(unreal.FrameRate(30,1),False);controller.set_number_of_frames(unreal.FrameNumber(4),False)
 for index,name in enumerate(tracks['frames'][0]['BoneNames']):
  rows=[f['LocalTransforms'][index] for f in tracks['frames']]
  assert all(f['BoneNames'][index]==name for f in tracks['frames'])
  assert controller.add_bone_curve(name,False),name
  assert controller.set_bone_track_keys(name,[unreal.Vector(*[t['Translation'][k] for k in 'XYZ']) for t in rows],[unreal.Quat(*[t['Rotation'][k] for k in 'XYZW']) for t in rows],[unreal.Vector(*[t['Scale3D'][k] for k in 'XYZ']) for t in rows],False),name
finally:controller.close_bracket(False)

def snapshot(pose,label):
 names=[str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)];transforms=[]
 for name in names:
  t=unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.LOCAL);v,q,z=t.translation,t.rotation,t.scale3d
  transforms.append(dict(Translation=dict(zip('XYZ',[v.x,v.y,v.z])),Rotation=dict(zip('XYZW',[q.x,q.y,q.z,q.w])),Scale3D=dict(zip('XYZ',[z.x,z.y,z.z]))))
 return dict(bIsValid=True,SkeletalMeshName='SK_Sester_Genessa_V6' if label=='control' else 'SK_SeduXtress_BodyRigV42' if label=='v42' else 'SK_SeduXtress_HandBindV43',BoneNames=names,LocalTransforms=transforms)

def canonical(raw):
 # JsonObjectConverter uses lower camel case for output fields.
 return dict(bIsValid=raw['bIsValid'],SkeletalMeshName=raw['skeletalMeshName'],BoneNames=raw['boneNames'],LocalTransforms=[dict(Translation={k:t['translation'][k.lower()] for k in 'XYZ'},Rotation={k:t['rotation'][k.lower()] for k in 'XYZW'},Scale3D={k:t['scale3D'][k.lower()] for k in 'XYZ'}) for t in raw['localTransforms']])
results=[]
for compressed in (False,True):
 if compressed:assert unreal.CSSRetargetLibrary.prepare_compressed_animation(animation,control)
 options=unreal.AnimPoseEvaluationOptions();options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.COMPRESSED if compressed else unreal.AnimDataEvalType.RAW);options.set_editor_property('should_retarget',True)
 for label,mesh in [('control',control),('foundation_css_modes',mixed),('v42',v42)]:
  options.set_editor_property('optional_skeletal_mesh',mesh)
  for sample in range(5):
   pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(animation,sample/30,options);before=snapshot(pose,label)
   check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(human)),diagnostic_source_bones)
   assert len(before['BoneNames'])==(267 if label=='control' else 388)
   (OUT/f'{label}-s{sample}-{"compressed" if compressed else "raw"}-input.json').write_text(json.dumps(dict(pose=dict(Snapshot=before)),indent=2)+'\n')
   for alpha in (0.,1.):
    text=unreal.CSSRetargetLibrary.probe_diagnostic_hand_ik(mesh,json.dumps(before),json.dumps(node),json.dumps(copy_nodes),alpha)
    assert text,(label,sample,compressed,alpha)
    data=json.loads(text);after=canonical(data['Snapshot'])
    doc=dict(pose=dict(Snapshot=after),scope='Original H2 sampled animation through Unreal retargeting, both actual CopyBone nodes and the actual player left TwoBoneIK node. Authored source and animated virtual tracks preserved. Excludes other gameplay graph stages.',source_sample=tracks['frames'][sample]['frame'],compressed=compressed,ik_alpha=alpha,effector_before_cm=data['effector_before_cm'],effector_after_cm=data['effector_after_cm'])
    name=f'{label}-s{sample}-{"compressed" if compressed else "raw"}-ik{int(alpha)}'
    (OUT/(name+'.json')).write_text(json.dumps(doc,indent=2)+'\n')
    results.append(dict(name=name,bones=len(after['BoneNames']),before_cm=data['effector_before_cm'],after_cm=data['effector_after_cm']))
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
(OUT/'evaluation.json').write_text(json.dumps(dict(production_unchanged=True,production_hashes=hashes,results=results),indent=2)+'\n')
unreal.log('CSS_GAME_FOUNDATION_H2_PROBE_PASSED')

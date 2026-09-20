"""UE5.6.1 bounded arm-binding comparison. No production save."""
import json, math, hashlib
from pathlib import Path
import unreal
import os
OUT = Path(os.environ['CSS_ARM_REST_AUDIT_DIR']).resolve()
if OUT.parent != Path(__file__).resolve().parents[3] / 'work/grip-grounding-v1':
    raise ValueError('Audit directory must be a direct workspace grip-grounding child')
ROOT=OUT.parents[3]
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
FIXTURE=OUT.parent/'game-foundation-v42-bind-v2'
references=json.loads((FIXTURE/'configured-references.json').read_text())
production=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSSAuthoring'
paths=[production/'Shared/Skeletons/SKEL_CSS_Base.uasset',production/'CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43.uasset']
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}

def check(actual,expected):
 assert len(actual)==len(expected)
 for a,b in zip(actual,expected,strict=True):
  assert (a['name'].lower(),a['parent'])==(b['name'].lower(),b['parent'])
  for key in ('translation','rotation','scale'):
   error=math.dist(a[key],b[key])
   if key=='rotation':error=min(error,math.dist(a[key],[-v for v in b[key]]))
   assert error<.001,(a['name'],key,error)

human=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameHumanMore1275_V1')
control=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_MoreBeaute258_V1')
original=unreal.load_asset('/Game/CSSAuthoring/CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43')
new_bind_mesh=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B_V1')
shared=unreal.load_asset('/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base')
rotations=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SKEL_CSSGameRotations_V1')
assert all((human,control,original,new_bind_mesh,shared,rotations))
shared=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SKEL_ArmRestProbeCSS','/Game/CSSAuthoring/DiagnosticReferences',shared)
assert shared
source_export=next(e for e in json.loads((OUT.parent/'live-source-skeleton-v1/decoded/SKEL_Human_Skeleton.json').read_text()) if e['Type']=='Skeleton')
diagnostic_source_bones=json.loads((FIXTURE/'source.mesh.json').read_text())['bones']
check(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(human)),diagnostic_source_bones)
modes={v.value:v for v in (unreal.BoneTranslationRetargetingMode.ANIMATION,unreal.BoneTranslationRetargetingMode.ANIMATION_SCALED,unreal.BoneTranslationRetargetingMode.SKELETON)}
for mode in {b['translation_mode'] for b in references['source']['bones']}:
 assert unreal.CSSRetargetLibrary.set_translation_retargeting(human,[b['name'] for b in references['source']['bones'] if b['translation_mode']==mode],modes[mode])
assert unreal.CSSRetargetLibrary.restore_virtual_bones(human,json.dumps(source_export['Properties']['VirtualBones']))
assert unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(control,human)
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(control)==9
new_bind=json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(new_bind_mesh))
expected=json.loads((MOD/'work/CSS_SeduXtress_ArmRestV44B.bindpose.json').read_text())
check(new_bind,expected)
(OUT/'engine-candidate-bind.json').write_text(json.dumps(new_bind,indent=2)+'\n')
old_bind=json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(original))
meshes=[]
for policy,skel in [('css',shared),('game_rotations',rotations)]:
 for mode in {b['translation_mode'] for b in references['target']['bones']}:
  assert unreal.CSSRetargetLibrary.set_translation_retargeting(skel,[b['name'] for b in references['target']['bones'] if b['translation_mode']==mode],modes[mode])
 assert unreal.CSSRetargetLibrary.restore_virtual_bones(skel,json.dumps(references['target']['virtual_bones']))
 skel.add_compatible_skeleton_soft(human)
 for label,src,bind in [('baseline',original,old_bind),('candidate',new_bind_mesh,new_bind)]:
  name=label+'_'+policy
  mesh=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SK_ArmRestProbe_'+name,'/Game/CSSAuthoring/DiagnosticReferences',src)
  assert mesh and unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh,skel)
  assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(mesh)==9
  check(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh)),bind)
  meshes.append((name,mesh))
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
animation=unreal.AssetToolsHelpers.get_asset_tools().create_asset('AN_ArmRestH2Samples_V1','/Game/CSSAuthoring/TransientProbes',unreal.AnimSequence,factory);assert animation
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
 return dict(bIsValid=True,SkeletalMeshName='SK_Sester_Genessa_V6' if label=='control' else 'SK_ArmRestV44B_V1' if label.startswith('candidate') else 'SK_SeduXtress_HandBindV43',BoneNames=names,LocalTransforms=transforms)

def canonical(raw):
 # JsonObjectConverter uses lower camel case for output fields.
 return dict(bIsValid=raw['bIsValid'],SkeletalMeshName=raw['skeletalMeshName'],BoneNames=raw['boneNames'],LocalTransforms=[dict(Translation={k:t['translation'][k.lower()] for k in 'XYZ'},Rotation={k:t['rotation'][k.lower()] for k in 'XYZW'},Scale3D={k:t['scale3D'][k.lower()] for k in 'XYZ'}) for t in raw['localTransforms']])
results=[]
for compressed in (False,True):
 if compressed:assert unreal.CSSRetargetLibrary.prepare_compressed_animation(animation,control)
 options=unreal.AnimPoseEvaluationOptions();options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.COMPRESSED if compressed else unreal.AnimDataEvalType.RAW);options.set_editor_property('should_retarget',True)
 for label,mesh in meshes:
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
unreal.log('CSS_ARM_REST_CANDIDATE_PROBE_PASSED')

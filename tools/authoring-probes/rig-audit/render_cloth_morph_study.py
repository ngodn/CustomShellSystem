import unreal,json,os,hashlib,time
from pathlib import Path
p=Path(os.environ['CSS_PANEL_STAGE'])
readback=os.environ.get('CSS_PANEL_READBACK')=='1'
all_shapes=os.environ.get('CSS_PANEL_ALL_SHAPES')=='1'
motion=os.environ.get('CSS_PANEL_MOTION')=='1'
held=os.environ.get('CSS_PANEL_HELD')=='1'
assert not motion or all_shapes
assert not held or motion
basis=all_shapes or os.environ.get('CSS_PANEL_BASIS')=='1'
run=os.environ.get('CSS_PANEL_RUN','')
assert not run or run.isdecimal()
suffix=('-motion'+run if motion else '-shapes'+run if all_shapes else '-basis' if basis else '')+('-readback' if readback else '')
if held:suffix='-held'+suffix
out=p/('captures'+suffix)
out.mkdir(exist_ok=True)
assert not any(out.iterdir()),f'Refusing to overwrite captures: {out}'
inputs=[Path(x) for x in json.loads((p/'sources.json').read_text())]
protected={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}
if motion:
 for f,h in json.loads((p/'motion-inputs.json').read_text()).items():
  assert hashlib.sha256(Path(f).read_bytes()).hexdigest()==h
  protected[f]=h
world=unreal.UGMotionLibrary.create_preview_world()
assert world
for cmd in ['r.AllowOcclusionQueries 0','r.AntiAliasingMethod 0','r.BloomQuality 0']:
 unreal.SystemLibrary.execute_console_command(world,cmd)

def material(name,color):
 existing=unreal.load_asset('/Game/CSS/UnholyGenessa/DeformerStudy/'+name) if unreal.EditorAssetLibrary.does_asset_exist('/Game/CSS/UnholyGenessa/DeformerStudy/'+name) else None
 if existing:return existing
 m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/CSS/UnholyGenessa/DeformerStudy',unreal.Material,unreal.MaterialFactoryNew())
 e=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
 e.set_editor_property('constant',unreal.LinearColor(*color))
 unreal.MaterialEditingLibrary.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 m.set_editor_properties(dict(shading_model=unreal.MaterialShadingModel.MSM_UNLIT,two_sided=True,used_with_skeletal_mesh=True,used_with_clothing=True))
 unreal.MaterialEditingLibrary.recompile_material(m)
 return m
body=material('M_Body',(.35,.12,.07,1));cloth=material('M_Cloth',(.08,.55,.25,1))
assert json.loads(unreal.UGMaterialLibrary.finish_materials([body,cloth]))['passed']
for diagnostic in [body,cloth]:assert unreal.EditorAssetLibrary.save_loaded_asset(diagnostic)
camera=unreal.UGMotionLibrary.spawn_preview_actor(unreal.SceneCapture2D)
cap=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
cap.set_editor_property('capture_every_frame',False);cap.set_editor_property('capture_on_movement',False)
cap.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
cap.set_editor_property('primitive_render_mode',unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
cap.set_editor_property('projection_type',unreal.CameraProjectionMode.ORTHOGRAPHIC);cap.set_editor_property('ortho_width',100.)
settings=cap.get_editor_property('post_process_settings')
settings.set_editor_property('override_auto_exposure_method',True);settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True);settings.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
cap.set_editor_property('post_process_settings',settings)
target=unreal.RenderingLibrary.create_render_target2d(world,512,512,unreal.TextureRenderTargetFormat.RTF_RGBA8,unreal.LinearColor(0,0,0,1))
cap.set_editor_property('texture_target',target)
trial=unreal.load_asset('/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy'+('4' if all_shapes else '3' if basis else '1'));assert trial
baseline=unreal.load_asset('/DeformerGraph/Deformers/DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals');assert baseline
shapes=['PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth','FBMBodyTone']
cases=[dict(name=str(int(x)),weights={'PBMGlutesSize':x},cloth=True) for x in [0.,1.]]
if all_shapes:
 cases=[dict(name='neutral',weights={},cloth=True)]
 cases += [dict(name=n,weights={m:1.},cloth=True) for n,m in zip(['glute','hip','thigh','waist','tone'],shapes)]
 cases += [dict(name='all-half',weights=dict.fromkeys(shapes,.5),cloth=True),
           dict(name='all-max',weights=dict.fromkeys(shapes,1.),cloth=True),
           dict(name='glute-disabled',weights={'PBMGlutesSize':1.},cloth=False),
           dict(name='all-disabled',weights=dict.fromkeys(shapes,1.),cloth=False),
           dict(name='neutral-disabled',weights={},cloth=False)]
if motion:
 clips={name:unreal.load_asset('/Game/CSS/eins0fx/Shared/Locomotion/EveLegacy/'+asset)
        for name,asset in [('idle','AN_I2_Idle'),('walk','AN_S1_Walk'),('jog','AN_S1_Jog'),('sprint','AN_S1_Sprint')]}
 assert all(clips.values())
 cases=[dict(name=f'{clip}-{shape}',clip=clip,weights=weights,cloth=True)
        for clip in clips for shape,weights in [('neutral',{}),('glute',{'PBMGlutesSize':1.})]]
modes=[('stock',baseline),('trial',trial)] if all_shapes else [('legacy',None),('stock',baseline),('trial',trial)]
rows=[]
def bone_location(component,name):
 assert component.get_bone_index(name)>=0,name
 value=component.get_socket_transform(name,unreal.RelativeTransformSpace.RTS_COMPONENT).translation
 return [value.x,value.y,value.z]
try:
 for variant in ['Eve','Genessa']:
  mesh=unreal.load_asset('/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_'+variant+'W3_Face'+('_BasisStudy2' if all_shapes else '_BasisStudy1' if basis else ''));assert mesh
  slots=list(mesh.get_editor_property('materials'))
  for case in cases:
   size=case['weights'].get('PBMGlutesSize',0.)
   for mode,graph in modes:
    advance_case=not held or mode==modes[0][0]
    if advance_case:
     actor=unreal.UGMotionLibrary.spawn_preview_actor(unreal.SkeletalMeshActor)
     comp=actor.get_component_by_class(unreal.SkeletalMeshComponent)
     comp.set_skeletal_mesh_asset(mesh);comp.set_forced_lod(1)
    assert unreal.CSSRigAuditLibrary.set_cloth_study_deformer(comp,graph)
    if advance_case:
     for i,slot in enumerate(slots):
      name=str(slot.get_editor_property('material_slot_name'))
      comp.set_material(i,cloth if name=='UG_Skirt' else body)
      comp.show_material_section(i,-1,name in ['UG_Skirt','UG_Skin2'],0)
     comp.set_editor_property('material_slots_overlay_material',[None]*len(slots))
     bone_samples=[]
    for frame in range((181 if motion else 91) if advance_case else 0):
     for morph in shapes:comp.set_morph_target(morph,case['weights'].get(morph,0.),False)
     state=unreal.CSSRigAuditLibrary.step_cloth_deformer_study(comp,frame==0,frame==0,not case['cloth'])
     assert all(abs(comp.get_morph_target(m)-case['weights'].get(m,0.))<1e-6 for m in shapes)
     if frame==0:assert json.loads(unreal.UGMaterialLibrary.finish_materials([body,cloth]))['passed']
     if state.startswith('ERROR:'):raise RuntimeError(state)
     if motion:
      if frame==0:comp.play_animation(clips[case['clip']],True)
      if frame in (1,60,120,180):
       bone_samples.append(dict(frame=frame,position=comp.get_position(),
                                bones={name:bone_location(comp,name)
                                       for name in ('pelvis','foot_l','foot_r')}))
    if held:
     state=unreal.CSSRigAuditLibrary.step_cloth_deformer_study(comp,False,False,False,False)
     if state.startswith('ERROR:'):raise RuntimeError(state)
    state=json.loads(state)
    if held:
     if advance_case:held_hash=state['cloth_state_sha1']
     assert state['cloth_state_sha1']==held_hash and not state['advanced_simulation'],state
    assert state['simulation_cloths']>0 and state['particles']>0,state
    assert state['cloth_disabled'] == (not case['cloth']),state
    if graph:assert state['deformer_instance'].split('.')[-1].startswith(graph.get_name()+'_Instance'),state
    if motion:
     assert len(bone_samples)==4 and any(sample['position']>0 for sample in bone_samples)
     state['motion_samples']=bone_samples
     state['animation']=clips[case['clip']].get_path_name()
    cap.show_only_actor_components(actor)
    geometry=None
    if readback and graph:
     label=('Motion'+run+'-' if motion else 'Shapes'+run+'-' if all_shapes else 'Basis-' if basis else '')+f"{variant}-{case['name']}-{mode}"
     if held:label='Held-'+label
     geometry=Path(unreal.Paths.project_saved_dir())/'DeformerReadback'/f'{label}.json'
     assert unreal.CSSRigAuditLibrary.request_cloth_deformer_readback(comp,label),label
     # The request is queued until the component sends fresh render data.
     updated=unreal.CSSRigAuditLibrary.step_cloth_deformer_study(comp,False,False,not case['cloth'],not held)
     if updated.startswith('ERROR:'):raise RuntimeError(updated)
     if held:assert json.loads(updated)['cloth_state_sha1']==held_hash
    for view,position in [('back',(-40,-300,100)),('side',(300,0,100))]:
     loc=unreal.Vector(*position)
     camera.set_actor_location_and_rotation(loc,unreal.MathLibrary.find_look_at_rotation(loc,unreal.Vector(0,0,95)),False,True)
     for _ in range(8):
      unreal.UGMotionLibrary.refresh_preview_rendering();cap.capture_scene();unreal.UGMaterialLibrary.flush_rendering()
     name=f"{variant}-{case['name']}-{mode}-{view}.png"
     unreal.RenderingLibrary.export_render_target(world,target,str(out),name)
     assert (out/name).is_file()
     rows.append(dict(variant=variant,case=case,glute=size,mode=mode,view=view,image=name,state=state))
    if geometry:
     deadline=time.monotonic()+30
     while not geometry.is_file() and time.monotonic()<deadline:
      unreal.UGMotionLibrary.refresh_preview_rendering()
      cap.capture_scene()
      unreal.UGMaterialLibrary.flush_rendering()
      time.sleep(.02)
     assert geometry.is_file(),f'GPU readback timed out: {geometry}'
     data=json.loads(geometry.read_text())
     assert data['passed'] and data['vertices']>0,data.keys()
     for row in rows[-2:]:row['gpu_readback']=str(geometry)
    cap.clear_show_only_components()
    if not held or mode==modes[-1][0]:actor.destroy_actor()
    (p/('progress'+suffix+'.json')).write_text(json.dumps(rows,indent=2))
finally:
 unreal.UGMaterialLibrary.flush_rendering();unreal.UGMotionLibrary.end_preview_world()
 assert all(hashlib.sha256(Path(f).read_bytes()).hexdigest()==h for f,h in protected.items())
(p/('result'+suffix+'.json')).write_text(json.dumps(dict(captures=rows,protected=protected,scope=('Offscreen Vulkan single-node gait playback' if motion else 'Offscreen Vulkan reference-pose cloth')+' with post-process disabled and flat diagnostic materials. No gameplay pose/material/performance acceptance.'),indent=2))

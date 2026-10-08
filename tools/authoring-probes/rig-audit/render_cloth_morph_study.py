import unreal,json,os,hashlib
from pathlib import Path
p=Path(os.environ['CSS_PANEL_STAGE']); out=p/'captures'
inputs=[Path(x) for x in json.loads((p/'sources.json').read_text())]
protected={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}
world=unreal.UGMotionLibrary.create_preview_world()
assert world
for cmd in ['r.AllowOcclusionQueries 0','r.AntiAliasingMethod 0','r.BloomQuality 0','r.SkinCache.CompileShaders 1']:
 unreal.SystemLibrary.execute_console_command(world,cmd)

def material(name,color):
 m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/CSS/UnholyGenessa/DeformerStudy',unreal.Material,unreal.MaterialFactoryNew())
 m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 m.set_editor_property('two_sided',True)
 m.set_editor_property('used_with_skeletal_mesh',True)
 m.set_editor_property('used_with_clothing',True)
 e=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
 e.set_editor_property('constant',unreal.LinearColor(*color))
 unreal.MaterialEditingLibrary.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 unreal.MaterialEditingLibrary.recompile_material(m)
 return m
body=material('M_Body',(.35,.12,.07,1));cloth=material('M_Cloth',(.08,.55,.25,1))
assert json.loads(unreal.UGMaterialLibrary.finish_materials([body,cloth]))['passed']
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
trial=unreal.load_asset('/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy1');assert trial
baseline=unreal.load_asset('/DeformerGraph/Deformers/DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals');assert baseline
rows=[]
try:
 for variant in ['Eve','Genessa']:
  mesh=unreal.load_asset('/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_'+variant+'W3_Face');assert mesh
  slots=list(mesh.get_editor_property('materials'))
  for size in [0.,1.]:
   for mode,graph in [('legacy',None),('stock',baseline),('trial',trial)]:
    actor=unreal.UGMotionLibrary.spawn_preview_actor(unreal.SkeletalMeshActor)
    comp=actor.get_component_by_class(unreal.SkeletalMeshComponent)
    comp.set_skeletal_mesh_asset(mesh);comp.set_forced_lod(1)
    comp.set_mesh_deformer(graph)
    for i,slot in enumerate(slots):
     name=str(slot.get_editor_property('material_slot_name'))
     comp.set_material(i,cloth if name=='UG_Skirt' else body)
     comp.show_material_section(i,-1,name in ['UG_Skirt','UG_Skin2'],0)
    comp.set_editor_property('material_slots_overlay_material',[None]*len(slots))
    for frame in range(91):
     comp.set_morph_target('PBMGlutesSize',size,False)
     state=unreal.CSSRigAuditLibrary.step_cloth_deformer_study(comp,frame==0,frame==0)
     assert abs(comp.get_morph_target('PBMGlutesSize')-size)<1e-6
     if frame==0:assert json.loads(unreal.UGMaterialLibrary.finish_materials([body,cloth]))['passed']
     if state.startswith('ERROR:'):raise RuntimeError(state)
    state=json.loads(state)
    assert state['simulation_cloths']>0 and state['particles']>0,state
    if graph:assert 'Optimus' in state['deformer_instance'],state
    cap.show_only_actor_components(actor)
    for view,position in [('back',(-40,-300,100)),('side',(300,0,100))]:
     loc=unreal.Vector(*position)
     camera.set_actor_location_and_rotation(loc,unreal.MathLibrary.find_look_at_rotation(loc,unreal.Vector(0,0,95)),False,True)
     for _ in range(8):
      unreal.UGMotionLibrary.refresh_preview_rendering();cap.capture_scene();unreal.UGMaterialLibrary.flush_rendering()
     name=f'{variant}-{size:g}-{mode}-{view}.png'
     unreal.RenderingLibrary.export_render_target(world,target,str(out),name)
     assert (out/name).is_file()
     rows.append(dict(variant=variant,glute=size,mode=mode,view=view,image=name,state=state))
    cap.clear_show_only_components();actor.destroy_actor()
    (p/'progress.json').write_text(json.dumps(rows,indent=2))
finally:
 unreal.UGMaterialLibrary.flush_rendering();unreal.UGMotionLibrary.end_preview_world()
 assert all(hashlib.sha256(Path(f).read_bytes()).hexdigest()==h for f,h in protected.items())
(p/'result.json').write_text(json.dumps(dict(captures=rows,protected=protected,scope='Offscreen Vulkan reference-pose cloth with post-process disabled and flat diagnostic materials. No gameplay pose/material/performance acceptance.'),indent=2))

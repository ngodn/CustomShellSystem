"""Fresh-load B2 reference candidate, defaults and actual component hand outputs."""
import hashlib
import json
import math
import sys
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=WORK/'b2-reference-fresh-v2'
OUT.mkdir(exist_ok=True);assert not (OUT/'report.json').exists()
HERE=Path(__file__).parent
sys.path[:0]=[str(HERE),str(ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools')]
from probe_controlrig_chain import key,set_value,transform
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
CONTENT=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
asset_path=lambda p:CONTENT/(p.removeprefix('/Game/')+'.uasset')
write=lambda n,data:(OUT/n).write_text(json.dumps(data,indent=2)+'\n')
packages=['/Game/CSSAuthoring/DiagnosticReferences/SKEL_B2GameReferenceMetadata_V2',
 '/Game/CSSAuthoring/DiagnosticReferences/SK_B2GameReferenceMetadata_V2',
 '/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeHandB2ReferenceV2']
protected=load(WORK/'hand-morph-import-regression-v1/report.json')['assets']
protected.update(load(WORK/'hand-postprocess-integration-v2/report.json')['protected_hashes'])
protected.update({str(asset_path(p)):digest(asset_path(p)) for p in packages})
assert all(digest(Path(p))==h for p,h in protected.items())
skeleton,mesh,bp=[unreal.load_asset(p) for p in packages];assert all((skeleton,mesh,bp))
source=unreal.load_asset('/Game/CSSAuthoring/CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43')
shared=unreal.load_asset('/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base')
assert mesh.get_editor_property('skeleton')==skeleton
assert mesh.get_editor_property('post_process_anim_blueprint')==bp.generated_class()
assert bp.get_editor_property('target_skeleton')==skeleton
metadata=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton_metadata(skeleton))
assert metadata==load(WORK/'b2-reference-metadata-v2/source-metadata.json')
assert metadata==json.loads(unreal.CSSRetargetLibrary.inspect_skeleton_metadata(shared))
assert json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(skeleton))==load(WORK/'b2-reference-metadata-v2/candidate-reference.json')
assert len(json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(skeleton)))==9
bones=load(WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json')
assert json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))==bones
assert len(mesh.get_editor_property('morph_targets'))==22
def materials(asset):
 rows=[]
 for slot in asset.get_editor_property('materials'):
  material=slot.get_editor_property('material_interface');overlay=slot.get_editor_property('overlay_material_interface')
  rows.append(dict(name=str(slot.material_slot_name),imported=str(slot.get_editor_property('imported_material_slot_name')),
   material=material.get_path_name() if material else None,overlay=overlay.get_path_name() if overlay else None,
   uv=str(slot.get_editor_property('uv_channel_data'))))
 return rows
actual_materials=materials(mesh);expected_materials=materials(source)
write('materials.json',dict(candidate=actual_materials,source=expected_materials))
# V44 geometry owns its UV density; preserve the material/name/overlay bindings.
for a,b in zip(actual_materials,expected_materials,strict=True):
 assert {k:v for k,v in a.items() if k!='uv'}=={k:v for k,v in b.items() if k!='uv'},(a,b)

physics=mesh.get_editor_property('physics_asset')
assert physics==source.get_editor_property('physics_asset')
cdo=unreal.get_default_object(bp.generated_class())
defaults={'CSSStiffness':200.,'CSSDamping':24.,'CSSHandEnabled':True,'CSSHandInputIsV43Compatible':False}
for name,value in defaults.items():assert cdo.get_editor_property(name)==value,(name,cdo.get_editor_property(name))
hand_names=[m['bone'] for m in load(HERE/'left-finger-calibration-v1.json')['parameters']]
curves=[m['shape'] for m in load(HERE/'left-finger-correctives-v1.json')['parameters']]
rig_bp=unreal.load_asset('/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1')
names=[b['name'] for b in bones]
def angle(a,b):
 dot=abs(sum(x*y for x,y in zip(a,b)))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
 return math.degrees(2*math.acos(min(1,dot)))
results=[]
for sample in range(5):
 path=WORK/'b2-animation-binding-v1'/f'candidate_game_rotations-s{sample}-compressed-ik1.json'
 raw=unreal.CSSControlRigLibrary.evaluate_hand_post_process_candidate(mesh,bp,json.dumps(load(path)['pose']['Snapshot']),curves,False)
 assert raw
 (OUT/f'h2-{sample}.component.json').write_text(raw+'\n');measured=json.loads(raw)
 assert all(f['mesh_morph'] for f in measured['curve_metadata'].values())
 rig=rig_bp.create_control_rig();h=rig.get_hierarchy()
 for row in measured['samples']:
  h.reset_pose_to_initial(unreal.RigElementType.BONE)
  for name,value in zip(names,row['upstream'],strict=True):
   pose=h.get_local_transform(key(name));pose.translation=unreal.Vector(*value['translation']);pose.rotation=unreal.Quat(*value['rotation']);pose.scale3d=unreal.Vector(*value['scale']);h.set_local_transform(key(name),pose,False,True)
  set_value(rig,'Enabled',row['enabled']);set_value(rig,'InputIsV43Compatible',False)
  assert rig.execute('Forwards Solve')
  max_angle=max_position=0.
  for name,upstream,output in zip(names,row['upstream'],row['output'],strict=True):
   expected=transform(h.get_local_transform(key(name))) if name in hand_names else upstream
   rotation=angle(output['rotation'],expected['rotation']);position=math.dist(output['translation'],expected['translation'])
   assert rotation<.001 and position<.0001 and math.dist(output['scale'],expected['scale'])<.00001,(sample,row['frame'],name)
   max_angle=max(max_angle,rotation);max_position=max(max_position,position)
  expected_curves={n:h.get_curve_value(unreal.RigElementKey(name=n,type=unreal.RigElementType.CURVE)) for n in curves}
  max_weight=max(abs(row['morph_weights'][n]-v) for n,v in expected_curves.items())
  assert max_weight<.0001
  for n,v in expected_curves.items():
   assert abs(row['curves'][n]-v)<.00001 and abs(row['rig_curves'][n]-v)<.00001
   if not row['enabled']:assert row['morph_weights'][n]==0
  results.append(dict(sample=sample,frame=row['frame'],maximum_rotation_degrees=max_angle,maximum_translation_cm=max_position,maximum_weight_error=max_weight))
assert len(results)==35 and all(digest(Path(p))==h for p,h in protected.items())
write('report.json',dict(passed=True,packages=packages,assets={str(asset_path(p)):digest(asset_path(p)) for p in packages},protected_hashes=protected,
 metadata_preserved=True,bones=379,virtual_bones=9,defaults=defaults,materials=30,morphs=22,
 physics_asset=physics.get_path_name() if physics else None,component_checks=results,
 scope='Fresh-load metadata, bind, materials, defaults and 35 actual component checks. Source Physics Asset is absent. No whole-game graph, collision, cook or live acceptance.'))
unreal.log('CSS_B2_REFERENCE_FRESH_PASSED')

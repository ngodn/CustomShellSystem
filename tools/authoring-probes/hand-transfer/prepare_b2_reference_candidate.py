"""Build an isolated metadata-preserving B2 binding, never the shared Skeleton."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=WORK/'b2-reference-metadata-v2'
CONTENT=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
asset_path=lambda p:CONTENT/(p.removeprefix('/Game/')+'.uasset')
write=lambda n,data:(OUT/n).write_text(json.dumps(data,indent=2)+'\n')
assert load(OUT/'build.exit.json')['exit_code']==0 and not (OUT/'report.json').exists()
protected=load(WORK/'hand-morph-import-regression-v1/report.json')['assets']
protected.update(load(WORK/'hand-postprocess-integration-v2/report.json')['protected_hashes'])
assert all(digest(Path(p))==h for p,h in protected.items())
shared=unreal.load_asset('/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base')
rotations=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SKEL_CSSGameRotations_V1')
original=unreal.load_asset('/Game/CSSAuthoring/CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43')
source=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B2ImportedMorphs_V1')
assert all((shared,rotations,original,source))
package='/Game/CSSAuthoring/DiagnosticReferences/SKEL_B2GameReferenceMetadata_V2'
assert not unreal.EditorAssetLibrary.does_asset_exist(package)
before=load(WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json')
shared_bones=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(shared))
rotation_bones=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(rotations))
metadata=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton_metadata(shared))
write('source-metadata.json',metadata)
skeleton=unreal.CSSRetargetLibrary.create_reference_rotation_candidate(shared,rotations,package)
assert skeleton
actual_metadata=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton_metadata(skeleton))
write('candidate-metadata.json',actual_metadata)
assert actual_metadata==metadata,'Skeleton metadata differs after duplication/reference change'
actual=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(skeleton))
for a,b,r in zip(actual,shared_bones,rotation_bones,strict=True):
 assert {k:v for k,v in a.items() if k!='rotation'}=={k:v for k,v in b.items() if k!='rotation'}
 assert a['rotation']==r['rotation'],a['name']
assert len(actual)==379
assert json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(skeleton))==json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(shared))
assert len(json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(skeleton)))==9
write('candidate-reference.json',actual)
mesh_package='/Game/CSSAuthoring/DiagnosticReferences/SK_B2GameReferenceMetadata_V2'
assert not unreal.EditorAssetLibrary.does_asset_exist(mesh_package)
mesh=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(mesh_package.rsplit('/',1)[1],mesh_package.rsplit('/',1)[0],source)
assert mesh and unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh,skeleton)
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(mesh)==9
assert json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))==before
slots=mesh.get_editor_property('materials');old=original.get_editor_property('materials')
assert [str(s.material_slot_name) for s in slots]==[str(s.material_slot_name) for s in old]
for index,(slot,previous) in enumerate(zip(slots,old,strict=True)):
 slot.set_editor_property('material_interface',previous.get_editor_property('material_interface'));slots[index]=slot
mesh.set_editor_property('materials',slots)
mesh.set_editor_property('physics_asset',original.get_editor_property('physics_asset'))
assert mesh.get_editor_property('physics_asset')==original.get_editor_property('physics_asset')
assert [s.get_editor_property('material_interface') for s in mesh.get_editor_property('materials')]==[s.get_editor_property('material_interface') for s in old]
assert len(mesh.get_editor_property('morph_targets'))==22
hand=unreal.load_asset('/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1')
baseline=unreal.load_asset('/Game/CSS/CSS_SeduXtress_eins0fx_P/ABP_SeduXtress_BodyHairV42')
names=[m['bone'] for m in load(Path(__file__).parent/'left-finger-calibration-v1.json')['parameters']]
curves=[m['shape'] for m in load(Path(__file__).parent/'left-finger-correctives-v1.json')['parameters']]
bp_package='/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeHandB2ReferenceV2'
bp=unreal.CSSControlRigLibrary.create_hand_post_process_candidate(mesh,baseline,hand,bp_package,names,curves)
assert bp
settings={'CSSStiffness':'200','CSSDamping':'24','CSSHandEnabled':'true','CSSHandInputIsV43Compatible':'false'}
assert unreal.CSSControlRigLibrary.configure_hand_candidate_defaults(bp)
cdo=unreal.get_default_object(bp.generated_class())
for name,value in {'CSSStiffness':200.,'CSSDamping':24.,'CSSHandEnabled':True,'CSSHandInputIsV43Compatible':False}.items():
 cdo.set_editor_property(name,value)
 assert cdo.get_editor_property(name)==value
mesh.set_editor_property('post_process_anim_blueprint',bp.generated_class())
assert unreal.EditorAssetLibrary.save_loaded_asset(skeleton,False)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
assert json.loads(unreal.CSSRetargetLibrary.inspect_skeleton_metadata(skeleton))==metadata
assert all(digest(Path(p))==h for p,h in protected.items())
write('report.json',dict(passed=True,packages=[package,mesh_package,bp_package],
 assets={str(asset_path(p)):digest(asset_path(p)) for p in (package,mesh_package,bp_package)},
 protected_hashes=protected,defaults=settings,metadata_objects=len(metadata),
 raw_bones=379,virtual_bones=9,materials=len(slots),morphs=22,
 physics_asset=mesh.get_editor_property('physics_asset').get_path_name() if mesh.get_editor_property('physics_asset') else None,
 scope='Isolated metadata/reference/material/postprocess bindings. Physics is copied only if the source has one; a null source is not collision acceptance. Fresh-process execution, cook and game verification still required.'))
unreal.log('CSS_B2_REFERENCE_METADATA_PASSED')

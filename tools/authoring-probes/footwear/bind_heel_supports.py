"""Bind isolated footwear mesh to accepted V44 assets, or verify after reload.

UE 5.6.1 editor Python. Never saves or changes the accepted source assets.
"""
import hashlib
import json
import os
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[4];WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
CONTENT=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
OUT=Path(os.environ['CSS_HEEL_BIND_OUTPUT']).resolve();assert OUT.parent==WORK and not OUT.exists();OUT.mkdir()
create=os.environ.get('CSS_HEEL_BIND_CREATE')=='1'
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
protected=load(WORK/'heel-support-import-v1/protected.json')
assert load(WORK/'heel-support-import-v1/exit.json')['exit_code']==0
assert all(digest(Path(p))==h for p,h in protected.items())
package='/Game/CSSAuthoring/DiagnosticReferences/SK_HeelSupportsV45C'
source=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_B2PhysicsBound_V1');mesh=unreal.load_asset(package);assert source and mesh
before=json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source))
assert json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))==before
skeleton=source.get_editor_property('skeleton')
metadata=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton_metadata(skeleton))
if create:
 assert unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh,skeleton)
 assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(mesh)==9
 slots=mesh.get_editor_property('materials');old=source.get_editor_property('materials')
 assert len(slots)==len(old)==30
 for index,(slot,prior) in enumerate(zip(slots,old,strict=True)):
  for name in ('material_slot_name','imported_material_slot_name'):assert slot.get_editor_property(name)==prior.get_editor_property(name)
  slot.set_editor_property('material_interface',prior.get_editor_property('material_interface'))
  # Python array iteration returns struct copies. Store each edited slot back.
  slots[index]=slot
 mesh.set_editor_property('materials',slots)
 for name in ('physics_asset','shadow_physics_asset','post_process_anim_blueprint'):
  mesh.set_editor_property(name,source.get_editor_property(name))
 assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
for name in ('skeleton','physics_asset','shadow_physics_asset','post_process_anim_blueprint'):
 assert mesh.get_editor_property(name)==source.get_editor_property(name),name
assert json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))==before
assert json.loads(unreal.CSSRetargetLibrary.inspect_skeleton_metadata(skeleton))==metadata
assert len(json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(skeleton)))==9
assert [str(m.get_name()) for m in mesh.get_editor_property('morph_targets')]==[str(m.get_name()) for m in source.get_editor_property('morph_targets')]
assert len(mesh.get_editor_property('morph_targets'))==22
for slot,prior in zip(mesh.get_editor_property('materials'),source.get_editor_property('materials'),strict=True):
 for name in ('material_slot_name','imported_material_slot_name','material_interface'):assert slot.get_editor_property(name)==prior.get_editor_property(name),(str(slot.material_slot_name),name)
assert all(digest(Path(p))==h for p,h in protected.items())
file=CONTENT/(package.removeprefix('/Game/')+'.uasset')
if not create:assert digest(file)==load(WORK/'heel-support-binding-v2/report.json')['mesh_sha256']
report=dict(passed=True,created=create,package=package,mesh_sha256=digest(file),protected_hashes=protected,bones=379,virtual_bones=9,materials=30,morphs=22,physics=mesh.get_editor_property('physics_asset').get_path_name(),post_process=mesh.get_editor_property('post_process_anim_blueprint').get_path_name(),scope='Saved footwear binding with accepted V44 rig/materials/physics/post-process retained. No game deployment or live support acceptance.')
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('HEEL_BIND_PASS',package,flush=True)

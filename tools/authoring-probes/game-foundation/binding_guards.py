import os
import unreal,json,hashlib
from pathlib import Path
OUT=Path(os.environ['CSS_FOUNDATION_AUDIT_DIR']).resolve()
root=OUT.parents[3]
assert OUT.parent == Path(__file__).resolve().parents[4]/'CustomShellSystem/work/grip-grounding-v1'
control=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_MoreBeaute258_V1')
missing=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameHuman1199_V1')
complete=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameHumanMore1275_V1')
production=unreal.load_asset('/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base')
assert all((control,missing,complete,production))
mesh=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('SK_BindingCoverageGuard_V1','/Game/CSSAuthoring/DiagnosticReferences',control)
assert mesh
before=mesh.get_editor_property('skeleton')
missing_before=unreal.CSSRetargetLibrary.inspect_skeleton(missing)
complete_before=unreal.CSSRetargetLibrary.inspect_skeleton(complete)
bind_before=unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh)
assert len(json.loads(missing_before))==1199 and len(json.loads(complete_before))==1275
rejected=not unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh,missing)
assert rejected and mesh.get_editor_property('skeleton')==before
assert unreal.CSSRetargetLibrary.inspect_skeleton(missing)==missing_before
assert not unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh,production)
accepted=unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh,complete)
assert accepted and mesh.get_editor_property('skeleton')==complete
assert unreal.CSSRetargetLibrary.inspect_skeleton(complete)==complete_before
assert unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh)==bind_before
result=dict(reject_missing_76_bones=rejected,reject_production_skeleton=True,rejected_binding_unchanged=True,source_references_unchanged=True,complete_binding_accepted=accepted,mesh_bind_unchanged=True)
(OUT/'binding-guard-results.json').write_text(json.dumps(result,indent=2)+'\n')
unreal.log('CSS_BINDING_COVERAGE_GUARDS_PASSED')

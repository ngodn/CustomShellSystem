"""Bind the isolated import-tie candidate to the verified short-path rig."""
import hashlib
import json
import os
from pathlib import Path
import unreal

CSS = Path(__file__).resolve().parents[2]
OUT = Path(os.environ['CSS_CLOSURE_WORK']).resolve()
assert OUT == CSS / 'work/paths/rig2' and not (OUT / 'bound.json').exists()
load = lambda p: json.loads(p.read_text())
assert load(OUT / 'import-exit.json')['exit_code'] == 0
protected = load(CSS / 'work/paths/rig1/verified.json')['protected_hashes']
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
source = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl')
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
assert source and mesh
inspect = unreal.CSSRetargetLibrary.inspect_mesh_bind_pose
assert json.loads(inspect(mesh)) == json.loads(inspect(source))
skeleton = source.get_editor_property('skeleton')
assert unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh, skeleton)
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(mesh) == 9
mesh.set_editor_property('materials', source.get_editor_property('materials'))
for key in ('physics_asset', 'shadow_physics_asset', 'post_process_anim_blueprint'):
    mesh.set_editor_property(key, source.get_editor_property(key))
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
assert json.loads(inspect(mesh)) == json.loads(inspect(source))
for key in ('skeleton', 'physics_asset', 'shadow_physics_asset', 'post_process_anim_blueprint'):
    assert mesh.get_editor_property(key) == source.get_editor_property(key), key
assert unreal.CSSRetargetLibrary.inspect_mesh_materials(mesh) == unreal.CSSRetargetLibrary.inspect_mesh_materials(source)
assert [str(m.get_name()) for m in mesh.get_editor_property('morph_targets')] == [str(m.get_name()) for m in source.get_editor_property('morph_targets')]
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
file = CSS.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSS/SeduXtress/SK_BlackPearl2.uasset'
(OUT / 'bound.json').write_text(json.dumps(dict(passed=True, mesh=mesh.get_path_name(),
    sha256=hashlib.sha256(file.read_bytes()).hexdigest(), protected_hashes=protected), indent=2) + '\n')
print('CSS_TIE_MESH_BOUND', flush=True)

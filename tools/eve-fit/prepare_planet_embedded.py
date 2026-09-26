"""Create a private fitted mesh with existing gameplay references for embedded cloth."""
import hashlib
import json
from pathlib import Path
import unreal

w=Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
output=w/'planet-embedded-prepare.json'
assert not output.exists()
path='/Game/CSS/EveTest/SK_PTailRun'
assert not unreal.EditorAssetLibrary.does_asset_exist(path)
source=unreal.load_asset('/Game/CSS/SeduXtress/SK_Eve_PlanetDiving')
fitted=unreal.load_asset('/Game/CSS/EveTest/SK_PlanetFit')
assert source and fitted
mesh=unreal.EditorAssetLibrary.duplicate_asset(fitted.get_path_name(),path)
assert mesh
refs={}
for prop in ('physics_asset','shadow_physics_asset','post_process_anim_blueprint'):
    value=source.get_editor_property(prop)
    if prop!='shadow_physics_asset': assert value, prop
    mesh.set_editor_property(prop,value)
    assert mesh.get_editor_property(prop)==value
    refs[prop]=value.get_path_name() if value else None
assert unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh)==unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(fitted)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
protected=json.loads((w/'planet-protected-before.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==sha for p,sha in protected.items())
output.write_text(json.dumps({'mesh':mesh.get_path_name(),'references':refs,'protected_unchanged':True,
    'scope':'Private candidate ready for cloth binding; not installed'},indent=2)+'\n')
unreal.log('PLANET_EMBEDDED_PREPARED')

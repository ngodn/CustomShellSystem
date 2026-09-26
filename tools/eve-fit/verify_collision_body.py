"""Read the saved private body mesh and compare its rig with the fitting source."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path('/home/eins0fx/development/mods/msII')
w = root/'CustomShellSystem/work/eve26'
output = w/'cbody-rig.json'
assert not output.exists()
protected = json.loads((w/'panel-motion-before.json').read_text())
actual = {p: hashlib.sha256((root/'CustomShellSystem'/p).read_bytes()).hexdigest() for p in protected}
assert actual == protected
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_CBody')
source = unreal.load_asset('/Game/CSS/EveTest/SK_Waist')
assert mesh and source
skeleton = mesh.get_editor_property('skeleton')
assert skeleton.get_path_name() == '/Game/CSS/EveTest/SKEL_CBody.SKEL_CBody'
rig = json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(skeleton))
assert rig == json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(source.get_editor_property('skeleton')))
bind = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))
assert bind == json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source))
assert not mesh.get_editor_property('physics_asset')
output.write_text(json.dumps({
    'mesh': mesh.get_path_name(), 'bones': len(rig),
    'skeleton_and_mesh_bind_match_private_source': True,
    'physics_asset_unassigned': True, 'protected_assets': actual,
    'scope': 'Saved private rig readback; not a replacement for shared 386-bone skeleton or collider coverage validation.'
}, indent=2)+'\n')

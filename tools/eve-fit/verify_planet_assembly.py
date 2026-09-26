"""Fresh-load checks for Prototype materials and production skeleton assignment."""
import hashlib
import json
from pathlib import Path
import unreal

work = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
out = work / 'planet-assembly-check.json'
assert not out.exists()
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_PlanetFit')
assert mesh
skeleton = mesh.get_editor_property('skeleton')
assert skeleton.get_path_name() == '/Game/CSS/Shared/SKEL_Base.SKEL_Base'
lib = unreal.CSSRetargetLibrary
bones = json.loads(lib.inspect_skeleton(skeleton))
sockets = json.loads(lib.inspect_sockets(skeleton))
virtual = json.loads(lib.inspect_virtual_bones(skeleton))
assert (len(bones), len(sockets), len(virtual)) == (386, 82, 9)
bind = json.loads(lib.inspect_mesh_bind_pose(mesh))
before = json.loads((work / 'planet-compat.json').read_text())
assert bind == before['mesh_bind'], 'Skeleton assignment changed the mesh bind pose'
materials = mesh.get_editor_property('materials')
expected = json.loads((work / 'planet-material-map.json').read_text())['slots']
assert len(materials) == len(expected) == 29
for row, actual in zip(expected, materials, strict=True):
    assert str(actual.material_slot_name) == row['name']
    assert actual.material_interface.get_path_name() == row['material'], (row['slot'], actual.material_interface.get_path_name(), row['material'])
protected = json.loads((work / 'planet-protected-before.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == sha for p, sha in protected.items())
out.write_text(json.dumps({'mesh': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(),
    'skeleton_bones': len(bones), 'sockets': len(sockets), 'virtual_bones': len(virtual),
    'mesh_bind_unchanged': True, 'verified_material_slots': len(materials),
    'protected_assets_unchanged': True,
    'scope': 'Fresh-load structure and references; textured appearance, physics and gameplay pending'}, indent=2) + '\n')
unreal.log('PLANET_ASSEMBLY_CHECK_DONE')

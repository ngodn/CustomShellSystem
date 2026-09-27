"""Restore production references on the isolated F13 mesh after skeleton binding."""
import hashlib,json
from pathlib import Path
import unreal

w=Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
report=w/'planet-f13-prepared.json'
assert not report.exists()
protected=json.loads((w/'planet-protected-before.json').read_text())
def check_protected():
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==sha for p,sha in protected.items())
check_protected()
mesh=unreal.load_asset('/Game/CSS/EveTest/SK_PFit13')
source=unreal.load_asset('/Game/CSS/SeduXtress/SK_Eve_PlanetDiving')
skeleton=unreal.load_asset('/Game/CSS/Shared/SKEL_Base')
assert mesh and source and skeleton and mesh.get_editor_property('skeleton')==skeleton
bind=unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh)
assert json.loads(bind)==json.loads((w/'planet-compat.json').read_text())['mesh_bind']
lib=unreal.CSSRetargetLibrary
counts=(len(json.loads(lib.inspect_skeleton(skeleton))),len(json.loads(lib.inspect_sockets(skeleton))),len(json.loads(lib.inspect_virtual_bones(skeleton))))
assert counts==(386,82,9)
mapping=json.loads((w/'planet-material-map.json').read_text())['slots']
slots=mesh.get_editor_property('materials')
assert len(slots)==len(mapping)==29
materials=[unreal.load_asset(row['material']) for row in mapping]
assert all(materials)
for row,material in zip(mapping,materials,strict=True):
    i=row['slot'];slot=slots[i]
    assert str(slot.material_slot_name)==row['name']
    slot.material_interface=material;slots[i]=slot
mesh.set_editor_property('materials',slots)
refs={}
for prop in ('physics_asset','shadow_physics_asset','post_process_anim_blueprint'):
    value=source.get_editor_property(prop)
    if prop!='shadow_physics_asset':assert value
    mesh.set_editor_property(prop,value)
    assert mesh.get_editor_property(prop)==value
    refs[prop]=value.get_path_name() if value else None
for row,slot in zip(mapping,mesh.get_editor_property('materials'),strict=True):
    assert slot.material_interface.get_path_name()==row['material']
assert unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh)==bind
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
check_protected()
report.write_text(json.dumps(dict(mesh=mesh.get_path_name(),references=refs,materials=mapping,skeleton_counts=counts,mesh_bind_unchanged=True,
    protected_unchanged=True,scope='F13 reference assignment saved; fresh reload and cloth binding still required.'),indent=2)+'\n')
unreal.log('PLANET_F13_PREPARED')

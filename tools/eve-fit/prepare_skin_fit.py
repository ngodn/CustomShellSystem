"""Restore current game references on the private Skin Suit fitting import."""
import hashlib,json,math
from pathlib import Path
import unreal
w=Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
report=w/'skin-f12-prepared.json';assert not report.exists()
protected=json.loads((w/'skin-protected-before.json').read_text())
def check_protected():
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==digest for p,digest in protected.items())
check_protected()
data=json.loads((w/'skin-f12-import/skin.mesh.json').read_text())
mesh=unreal.load_asset('/Game/CSS/EveTest/SK_SFit12')
source=unreal.load_asset('/Game/CSS/SeduXtress/SK_Eve_SkinSuit')
skeleton=unreal.load_asset('/Game/CSS/Shared/SKEL_Base')
assert mesh and source and skeleton and mesh.get_editor_property('skeleton')==skeleton
lib=unreal.CSSRetargetLibrary
bind=json.loads(lib.inspect_mesh_bind_pose(mesh))
assert len(bind)==len(data['bones'])
for actual,expected in zip(bind,data['bones'],strict=True):
    assert actual['name']==expected['name'] and actual['parent']==expected['parent']
    assert max(abs(a-b) for a,b in zip(actual['translation'],expected['translation']))<.0001
    assert max(abs(a-b) for a,b in zip(actual['scale'],expected['scale']))<.00001
    q=actual['rotation'];r=expected['rotation']
    dot=sum(a*b for a,b in zip(q,r))/math.sqrt(sum(a*a for a in q)*sum(b*b for b in r))
    assert abs(dot)>1-.00001
counts=(len(json.loads(lib.inspect_skeleton(skeleton))),len(json.loads(lib.inspect_sockets(skeleton))),len(json.loads(lib.inspect_virtual_bones(skeleton))))
assert counts==(386,82,9)
slots=mesh.get_editor_property('materials');original=source.get_editor_property('materials')
assert len(slots)==24 and len(original)==20
mapping=list(range(20))+[1,6,2,6]
materials=[]
for i,src in enumerate(mapping):
    assert str(slots[i].material_slot_name)==data['materials'][i]
    material=original[src].material_interface;assert material
    slot=slots[i];slot.material_interface=material;slots[i]=slot
    materials.append(dict(slot=i,name=data['materials'][i],source_slot=src,material=material.get_path_name()))
mesh.set_editor_property('materials',slots)
refs={}
for prop in ('physics_asset','shadow_physics_asset','post_process_anim_blueprint'):
    value=source.get_editor_property(prop)
    if prop!='shadow_physics_asset':assert value
    mesh.set_editor_property(prop,value);refs[prop]=value.get_path_name() if value else None
assert json.loads(lib.inspect_mesh_bind_pose(mesh))==bind
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
check_protected()
report.write_text(json.dumps(dict(mesh=mesh.get_path_name(),materials=materials,references=refs,
    skeleton_counts=counts,bind_preserved=True,protected_unchanged=True,
    scope='Private saved reference assignment. Requires fresh reload, visibility/morph/motion and game verification.'),indent=2)+'\n')
unreal.log('SKIN_F12_PREPARED')

"""Bind materials and physics to the isolated Bikini fitting mesh."""
import hashlib
import json
import math
from pathlib import Path
import unreal

work = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
output = work / 'bikini-prepared1.json'
assert not output.exists()
protected = json.loads((work / 'bikini-import1/protected.json').read_text())
def check_protected():
    assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest for path,digest in protected.items())
check_protected()
data = json.loads((work / 'bikini-import1/bikini.mesh.json').read_text())
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_BFit1')
source = unreal.load_asset('/Game/CSS/SeduXtress/SK_Eve_Bikini')
skeleton = unreal.load_asset('/Game/CSS/Shared/SKEL_Base')
blueprint = unreal.load_asset('/Game/CSS/EveTest/ABP_BikiniFeet')
assert mesh and source and skeleton and blueprint
assert mesh.get_editor_property('skeleton') == skeleton
lib = unreal.CSSRetargetLibrary
bind = json.loads(lib.inspect_mesh_bind_pose(mesh))
assert len(bind) == len(data['bones'])
for actual, expected in zip(bind, data['bones'], strict=True):
    assert actual['name'] == expected['name'] and actual['parent'] == expected['parent']
    for field, tolerance in [('translation', .0001), ('scale', .00001)]:
        assert max(abs(a-b) for a,b in zip(actual[field], expected[field])) < tolerance
    q,r = actual['rotation'],expected['rotation']
    dot = sum(a*b for a,b in zip(q,r))/math.sqrt(sum(a*a for a in q)*sum(b*b for b in r))
    assert abs(dot) > 1-.00001
counts = [len(json.loads(method(skeleton))) for method in (lib.inspect_skeleton, lib.inspect_sockets, lib.inspect_virtual_bones)]
assert counts == [386,82,9]
slots = mesh.get_editor_property('materials')
original = source.get_editor_property('materials')
assert len(slots) == len(original) == len(data['materials']) == 32
materials = []
for i,slot in enumerate(slots):
    assert str(slot.material_slot_name) == data['materials'][i]
    material = original[i].material_interface
    assert material
    slot.material_interface = material
    slots[i] = slot
    materials.append(material.get_path_name())
mesh.set_editor_property('materials', slots)
refs = {}
for prop in ('physics_asset', 'shadow_physics_asset'):
    value = source.get_editor_property(prop)
    if prop == 'physics_asset': assert value
    mesh.set_editor_property(prop, value)
    refs[prop] = value.get_path_name() if value else None
generated = unreal.load_class(None, '/Game/CSS/EveTest/ABP_BikiniFeet.ABP_BikiniFeet_C')
assert generated
mesh.set_editor_property('post_process_anim_blueprint', generated)
refs['post_process_anim_blueprint'] = generated.get_path_name()
assert json.loads(lib.inspect_mesh_bind_pose(mesh)) == bind
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
check_protected()
output.write_text(json.dumps(dict(mesh=mesh.get_path_name(), materials=materials,
    references=refs, skeleton_counts=counts, bind_preserved=True, protected_unchanged=True,
    scope='Private saved candidate. Fresh reload and game verification remain required.'), indent=2)+'\n')
print('BIKINI_PREPARED', flush=True)

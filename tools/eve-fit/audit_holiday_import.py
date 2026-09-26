"""Check private Holiday import compatibility without saving Unreal assets."""
import json
from pathlib import Path
import unreal

WORK = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
output = WORK / 'holiday-compat.json'
assert not output.exists()
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_Holiday')
shared = unreal.load_asset('/Game/CSS/Shared/SKEL_Base')
assert mesh and shared
private = mesh.get_editor_property('skeleton')
assert private.get_path_name() == '/Game/CSS/EveTest/SKEL_Holiday_Import.SKEL_Holiday_Import'
imported = json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(private))
current = json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(shared))
sockets = json.loads(unreal.CSSRetargetLibrary.inspect_sockets(shared))
virtual = json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(shared))
lookup = {bone['name']: bone for bone in current}
assert len(lookup) == len(current)
for bone in imported:
    target = lookup[bone['name']]
    parent = imported[bone['parent']]['name'] if bone['parent'] >= 0 else None
    target_parent = current[target['parent']]['name'] if target['parent'] >= 0 else None
    assert parent == target_parent, (bone['name'], parent, target_parent)
report = {
    'mesh': mesh.get_path_name(),
    'private_bones': len(imported),
    'shared_bones': len(current),
    'shared_sockets': len(sockets),
    'shared_virtual_bones': len(virtual),
    'matching_parent_names': True,
    'additional_shared_bones': [b['name'] for b in current if b['name'] not in {i['name'] for i in imported}],
    'materials': [str(m.get_editor_property('material_slot_name')) for m in mesh.get_editor_property('materials')],
    'clothing_assets': [a.get_path_name() for a in mesh.get_editor_property('mesh_clothing_assets')],
    'scope': 'Name and hierarchy compatibility only; no bind, cloth or game acceptance',
}
output.write_text(json.dumps(report, indent=2) + '\n')
unreal.log('HOLIDAY_COMPATIBILITY_CHECK_DONE')

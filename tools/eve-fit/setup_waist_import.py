"""Configure the isolated verified waist import without changing its reference assets."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path('/home/eins0fx/development/mods/msII')
w = root/'CustomShellSystem/work/eve26'
output = w/'waist-native-setup.json'
assert not output.exists()
protected = json.loads((w/'panel-motion-before.json').read_text())
def hashes():
    return {p: hashlib.sha256((root/'CustomShellSystem'/p).read_bytes()).hexdigest() for p in protected}
assert hashes() == protected
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_Waist')
baseline = unreal.load_asset('/Game/CSS/EveTest/SK_Holiday')
assert mesh and baseline
private = mesh.get_editor_property('skeleton')
assert private.get_path_name() == '/Game/CSS/EveTest/SKEL_Waist.SKEL_Waist'
current = json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(private))
original = json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(baseline.get_editor_property('skeleton')))
assert current == original, 'Imported reference skeleton differs from baseline'
materials = list(mesh.get_editor_property('materials'))
old = {str(m.get_editor_property('material_slot_name')): m.get_editor_property('material_interface') for m in baseline.get_editor_property('materials')}
for slot in materials:
    name = str(slot.get_editor_property('material_slot_name'))
    assert name in old and old[name]
    slot.set_editor_property('material_interface', old[name])
mesh.set_editor_property('materials', materials)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
assert hashes() == protected
output.write_text(json.dumps({'mesh': mesh.get_path_name(), 'bones': len(current),
    'reference_skeleton_identical_to_private_baseline': True, 'material_slots': len(materials),
    'protected_assets': protected, 'scope': 'Private material setup and skeleton readback only; no cloth or game acceptance.'}, indent=2)+'\n')

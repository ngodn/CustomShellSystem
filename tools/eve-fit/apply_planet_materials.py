"""Restore original material references on the private fitted Prototype mesh."""
import hashlib
import json
from pathlib import Path
import unreal

work = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
report = work / 'planet-material-apply.json'
assert not report.exists()
data = json.loads((work / 'planet-material-map.json').read_text())
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_PlanetFit')
assert mesh
slots = mesh.get_editor_property('materials')
assert len(slots) == len(data['slots']) == 29
for row in data['slots']:
    index = row['slot']
    assert str(slots[index].material_slot_name) == row['name']
    material = unreal.load_asset(row['material'])
    assert material
    slots[index].material_interface = material
mesh.set_editor_property('materials', slots)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
protected = json.loads((work / 'planet-protected-before.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == sha for p, sha in protected.items())
report.write_text(json.dumps({'mesh': mesh.get_path_name(), 'slots': data['slots'],
    'protected_assets_unchanged': True, 'scope': 'Material assignment saved; fresh reload and visual verification pending'}, indent=2) + '\n')
unreal.log('PLANET_MATERIAL_APPLY_DONE')

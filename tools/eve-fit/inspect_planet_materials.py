"""Resolve Prototype material aliases from the existing production mesh, read-only."""
import hashlib
import json
from pathlib import Path
import unreal

work = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
output = work / 'planet-material-map.json'
assert not output.exists()
source = unreal.load_asset('/Game/CSS/SeduXtress/SK_Eve_PlanetDiving')
target = unreal.load_asset('/Game/CSS/EveTest/SK_PlanetFit')
assert source and target
aliases = json.loads((work / 'planet-sections/material-aliases.json').read_text())
original = source.get_editor_property('materials')
rows = []
for index, slot in enumerate(target.get_editor_property('materials')):
    source_index = aliases[str(index)]['source_slot'] if str(index) in aliases else index
    old = original[source_index]
    expected = aliases[str(index)]['source_material'] if str(index) in aliases else str(slot.material_slot_name)
    assert str(old.material_slot_name) == expected, (index, expected, str(old.material_slot_name))
    material = old.material_interface
    assert material, (source_index, expected)
    rows.append({'slot': index, 'name': str(slot.material_slot_name),
                 'source_slot': source_index, 'material': material.get_path_name()})
protected = json.loads((work / 'planet-protected-before.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == sha for p, sha in protected.items())
output.write_text(json.dumps({'source': source.get_path_name(), 'slots': rows,
    'protected_assets_unchanged': True, 'scope': 'Resolved references only; no assets saved or appearance verified'}, indent=2) + '\n')
unreal.log('PLANET_MATERIAL_MAP_DONE')

"""Assign existing Holiday materials and body secondary assets to the private test mesh."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path('/home/eins0fx/development/mods/msII')
WORK = ROOT / 'CustomShellSystem/work/eve26'
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSS'
report_path = WORK / 'holiday-materials.json'
assert not report_path.exists()
protected = [CONTENT / 'Shared/SKEL_Base.uasset', CONTENT / 'SeduXtress/SK_Eve_Christmas.uasset']
before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_Holiday')
source = unreal.load_asset('/Game/CSS/SeduXtress/SK_Eve_Christmas')
assert mesh and source
reference = {}
for slot in source.get_editor_property('materials'):
    name = str(slot.get_editor_property('material_slot_name'))
    assert name not in reference
    reference[name] = slot.get_editor_property('material_interface')

# These suffixes come from the four separately appended source garment objects.
aliases = {
    'MI_CH_P_EVE_Christmas_01_Decal.001': 'MI_CH_P_EVE_Christmas_01_Decal',
    'MI_CH_P_EVE_Christmas_01_01.001': 'MI_CH_P_EVE_Christmas_01_01',
    'MI_EVE_HR_Christmas_01_Fur.001': 'MI_EVE_HR_Christmas_01_Fur',
    'MI_EVE_HR_15_Emissive1.001': 'MI_EVE_HR_15_Emissive1',
    'MI_CH_P_EVE_Christmas_01_03.001': 'MI_CH_P_EVE_Christmas_01_03',
    'MI_CH_P_EVE_Christmas_01_02.003': 'MI_CH_P_EVE_Christmas_01_02',
    'MI_CH_P_EVE_Christmas_01_03.003': 'MI_CH_P_EVE_Christmas_01_03',
    'MI_CH_P_EVE_Christmas_01_01.003': 'MI_CH_P_EVE_Christmas_01_01',
    'MI_CH_P_EVE_Christmas_01_Decal.003': 'MI_CH_P_EVE_Christmas_01_Decal',
    'MI_CH_P_EVE_Christmas_01_02.004': 'MI_CH_P_EVE_Christmas_01_02',
    'MI_CH_P_EVE_Christmas_01_01.004': 'MI_CH_P_EVE_Christmas_01_01',
    'MI_EVE_HR_15_Emissive1.004': 'MI_EVE_HR_15_Emissive1',
    'MI_CH_P_EVE_Christmas_01_01.005': 'MI_CH_P_EVE_Christmas_01_01',
    'MI_CH_P_EVE_Christmas_01_Decal.005': 'MI_CH_P_EVE_Christmas_01_Decal',
}
rows = []
materials = list(mesh.get_editor_property('materials'))
for slot in materials:
    name = str(slot.get_editor_property('material_slot_name'))
    original = aliases.get(name, name)
    assert original in reference and reference[original], (name, original)
    slot.set_editor_property('material_interface', reference[original])
    rows.append({'slot': name, 'source_slot': original, 'material': reference[original].get_path_name()})
mesh.set_editor_property('materials', materials)
for prop in ('physics_asset', 'shadow_physics_asset', 'post_process_anim_blueprint'):
    mesh.set_editor_property(prop, source.get_editor_property(prop))
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
after = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
assert before == after, 'A protected production asset changed'
report_path.write_text(json.dumps({
    'materials': rows, 'protected_sha256': before,
    'scope': 'Existing material assignments reused; texture appearance and cloth not yet validated',
}, indent=2) + '\n')
unreal.log('HOLIDAY_MATERIAL_SETUP_DONE')

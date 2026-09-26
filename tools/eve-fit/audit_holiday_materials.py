"""Read the private Holiday garment's material ancestry without changing assets."""
import json
from pathlib import Path

import unreal

ROOT = Path('/home/eins0fx/development/mods/msII')
OUT = ROOT / 'CustomShellSystem/work/eve26/cloth-materials.json'
assert not OUT.exists(), OUT
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_Holiday')
assert mesh
names = {
    'MI_CH_P_EVE_Christmas_01_01.001',
    'MI_CH_P_EVE_Christmas_01_Decal.001',
    'MI_EVE_HR_Christmas_01_Fur.001',
    'MI_EVE_HR_15_Emissive1.001',
    'MI_CH_P_EVE_Christmas_01_03.001',
}
rows = []
for slot in mesh.get_editor_property('materials'):
    name = str(slot.get_editor_property('material_slot_name'))
    if name not in names:
        continue
    mat = slot.get_editor_property('material_interface')
    chain, seen = [], set()
    while mat:
        path = mat.get_path_name()
        assert path not in seen, path
        seen.add(path)
        row = {'asset': path, 'class': mat.get_class().get_name()}
        if isinstance(mat, unreal.MaterialInstanceConstant):
            for kind in ('scalar', 'vector', 'texture'):
                values = []
                for param in mat.get_editor_property(kind + '_parameter_values'):
                    info = param.get_editor_property('parameter_info')
                    value = param.get_editor_property('parameter_value')
                    if kind == 'vector':
                        value = [value.r, value.g, value.b, value.a]
                    elif kind == 'texture':
                        value = value.get_path_name() if value else None
                    values.append({'name': str(info.get_editor_property('name')),
                                   'association': str(info.get_editor_property('association')),
                                   'index': info.get_editor_property('index'), 'value': value})
                row[kind] = values
            row['base_overrides'] = str(mat.get_editor_property('base_property_overrides'))
            mat = mat.get_editor_property('parent')
        else:
            assert isinstance(mat, unreal.Material), row
            row['blend_mode'] = str(mat.get_editor_property('blend_mode'))
            row['two_sided'] = mat.get_editor_property('two_sided')
            row['shading_model'] = str(mat.get_editor_property('shading_model'))
            mat = None
        chain.append(row)
    rows.append({'slot': name, 'ancestry': chain})
assert {r['slot'] for r in rows} == names
OUT.write_text(json.dumps({'materials': rows, 'scope': 'Saved material properties, not rendered appearance'}, indent=2) + '\n')
unreal.log('HOLIDAY_CLOTH_MATERIAL_AUDIT_DONE')

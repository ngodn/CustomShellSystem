"""Read saved Eve mesh cloth assignments without editing or saving assets."""
import json
from pathlib import Path
import unreal

output=Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26/ue-cloth.json')
assert not output.exists()
rows={}
for name in ('SK_BlackPearl2','SK_Eve_Christmas','SK_Eve_YoRHa'):
 mesh=unreal.load_asset('/Game/CSS/SeduXtress/'+name)
 assert mesh,name
 row={}
 for prop in ('skeleton','physics_asset','post_process_anim_blueprint'):
  value=mesh.get_editor_property(prop)
  row[prop]=value.get_path_name() if value else None
 assets=mesh.get_editor_property('mesh_clothing_assets')
 row['clothing_assets']=[asset.get_path_name() for asset in assets]
 row['materials']=[str(slot.get_editor_property('material_slot_name')) for slot in mesh.get_editor_property('materials')]
 rows[name]=row
output.write_text(json.dumps(rows,indent=2)+'\n')
unreal.log('EVE_CLOTH_AUDIT_DONE')

"""Read the current secondary-motion collision asset and saved Holiday references."""
import json
from pathlib import Path
import unreal

WORK = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
output = WORK / 'holiday-collision.json'
assert not output.exists()
asset = unreal.load_asset('/Game/CSS/SeduXtress/PA_Body')
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_Holiday')
assert asset and mesh
raw = unreal.CSSPhysicsProbeLibrary.inspect_body_candidate(asset)
assert raw, 'Physics inspection returned no data'
report = json.loads(raw)
report['mesh_references'] = {}
for name in ('skeleton', 'physics_asset', 'shadow_physics_asset', 'post_process_anim_blueprint'):
    value = mesh.get_editor_property(name)
    report['mesh_references'][name] = value.get_path_name() if value else None
output.write_text(json.dumps(report, indent=2)+'\n')
unreal.log('HOLIDAY_COLLISION_AUDIT_DONE')

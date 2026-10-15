import json
from pathlib import Path
import unreal
root = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/npc-appearances/bosses')
for plan in json.loads((root / 'selected-plan.json').read_text()):
    skeleton = unreal.load_asset(plan['skeleton'])
    assert skeleton
    assert unreal.CSSNpcLibrary.clear_boss_preview(skeleton)
    assert unreal.EditorAssetLibrary.save_loaded_asset(skeleton, False)
unreal.log('CSS_BOSS_PREVIEWS_CLEARED')

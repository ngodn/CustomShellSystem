"""Stage the fourteen verified E5 sequences at their package-facing paths."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
assert not (WORK/'export-result.json').exists()
receipt = json.loads((WORK/'import-result.json').read_text())
assert [r['clip'] for r in receipt['results']] == list(range(700, 714))
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
protected = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in (content/'CSS').rglob('*.uasset')}
names = ['Standing Poise', 'Kneeling Poise', 'Seated Recline', 'Kneeling Bow',
         'Low Lean', 'Deep Kneeling Bow', 'Reclining', 'Forward Lean',
         'Forward Fold', 'Kneeling Hands Back', 'Arched Stretch',
         'Crouched Hands Together', 'Deep Squat', 'Side Kneel']
tools = unreal.AssetToolsHelpers.get_asset_tools()
entries = []
for row, name in zip(receipt['results'], names, strict=True):
    clip = row['clip']
    source = unreal.load_asset(row['asset'])
    folder = '/Game/CSS/Eve/Anim/Idles'
    asset_name = f'AN_Idle{clip}'
    assert source and not unreal.EditorAssetLibrary.does_asset_exist(folder+'/'+asset_name)
    output = tools.duplicate_asset(asset_name, folder, source)
    assert output and abs(output.get_play_length()-100/30) < .0001
    assert output.get_editor_property('skeleton') == source.get_editor_property('skeleton')
    assert unreal.EditorAssetLibrary.save_loaded_asset(output, False)
    entries.append(dict(id=f'eve_idle_{clip}', name='Eve '+name,
                        clip=output.get_path_name(), hide_weapons=True))
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
(WORK/'export-result.json').write_text(json.dumps(dict(entries=entries, protected=protected,
    scope='Uncooked staging only; visual review and runtime acceptance remain pending'), indent=2)+'\n')

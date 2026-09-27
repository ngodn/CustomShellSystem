"""Stage the verified deep-squat height revision without replacing existing assets."""
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[3]
WORK = ROOT/'work/eve-idle1'
result = WORK/'e6/export-result.json'
assert not result.exists()
receipt = json.loads((WORK/'e6/import-result.json').read_text())
assert [r['clip'] for r in receipt['results']] == [712]
contact = json.loads((WORK/'e6-contact712/report.json').read_text())
assert len(contact['errors']) == 101
assert all(1 < row['contact_lowest_cm'] < 3.5 for row in contact['errors'])
content = ROOT.parent/'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSS'
protected = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in content.rglob('*.uasset')}
source = unreal.load_asset(receipt['results'][0]['asset'])
folder = '/Game/CSS/Eve/Anim/Idles'
name = 'AN_Idle712B'
assert source and not unreal.EditorAssetLibrary.does_asset_exist(folder+'/'+name)
output = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, folder, source)
assert output and abs(output.get_play_length()-100/30) < .0001
assert output.get_editor_property('skeleton') == source.get_editor_property('skeleton')
assert unreal.EditorAssetLibrary.save_loaded_asset(output, False)
entries = json.loads((WORK/'e5/export-result.json').read_text())['entries']
entry = next(e for e in entries if e['id'] == 'eve_idle_712')
entry['clip'] = output.get_path_name()
entry['name'] = 'Eve Seated Tuck'
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p,h in protected.items())
result.write_text(json.dumps(dict(entries=entries, protected=protected,
    scope='Deep-squat root-height correction, gameplay acceptance pending'), indent=2)+'\n')
(WORK/'cook-list2.txt').write_text(''.join(e['clip'].split('.')[0]+'\n' for e in entries))

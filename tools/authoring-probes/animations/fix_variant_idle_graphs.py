"""Add idle playback to the two accepted footwear graphs. UE 5.6.1."""
import hashlib
import json
from pathlib import Path
import shutil
import unreal

ROOT = Path(__file__).resolve().parents[3]
WORK = ROOT/'work/eve-idle1/variant-bug'
CONTENT = ROOT.parent/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
names = ['ABP_BikiniFeet2', 'ABP_KnitFeet1']
paths = [CONTENT/'CSS/EveTest'/f'{name}.uasset' for name in names]
backup = WORK/'source-backup'
backup.mkdir(exist_ok=False)
protected = {str(p):hashlib.sha256(p.read_bytes()).hexdigest()
             for p in (CONTENT/'CSS').rglob('*.uasset') if p not in paths}
before_hashes = {}
for p in paths:
    shutil.copy2(p, backup/p.name)
    before_hashes[str(p)] = hashlib.sha256(p.read_bytes()).hexdigest()
(backup/'manifest.json').write_text(json.dumps(before_hashes,indent=2)+'\n')
rows = []
for name in names:
    package = '/Game/CSS/EveTest/'+name
    source = unreal.load_asset(package)
    assert source
    before = json.loads(unreal.CSSRetargetLibrary.inspect_pose_corrections(source))
    fixed = unreal.CSSIdleLibrary.create_idle_layer(source, package)
    assert fixed == source
    after = json.loads(unreal.CSSRetargetLibrary.inspect_pose_corrections(fixed))
    assert before == after, 'Footwear controls changed'
    assert unreal.EditorAssetLibrary.save_loaded_asset(fixed, False)
    rows.append(dict(asset=package, footwear_controls=after))
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in protected.items())
(WORK/'repair.json').write_text(json.dumps(dict(graphs=rows, protected=protected,
    scope='Compiled idle branch added; footwear controls preserved; cooked and runtime verification pending'),indent=2)+'\n')
(WORK/'cook.txt').write_text(''.join(row['asset']+'\n' for row in rows))

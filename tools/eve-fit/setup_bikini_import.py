"""Prepare the verified fitting interchange for an isolated Unreal import."""
import hashlib
import json
import argparse
from pathlib import Path

root = Path(__file__).resolve().parents[3]
work = root / 'CustomShellSystem/work/eve26'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--kind', choices=('bikini', 'knit'), default='bikini')
kind = parser.parse_args().kind
source = work / ('bikini-anklew1/bikini.mesh.json' if kind == 'bikini' else 'knit-export2/knit.mesh.json')
raw = source.read_bytes()
audit = json.loads(source.with_suffix('.audit.json').read_text())
assert hashlib.sha256(raw).hexdigest() == audit['output_sha256']
data = json.loads(raw)
suffix = 'BFit1' if kind == 'bikini' else 'KFit2'
data['mesh_package'] = '/Game/CSS/EveTest/SK_' + suffix
data['skeleton_package'] = '/Game/CSS/EveTest/SKEL_' + suffix
out = work / (kind + '-import1')
out.mkdir(exist_ok=False)
content = root / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSS'
protected = {}
original = 'SK_Eve_Bikini' if kind == 'bikini' else 'SK_Eve_CasualSweater'
for relative in ('Shared/SKEL_Base', 'SeduXtress/PA_Body', 'SeduXtress/ABP_Secondary', 'SeduXtress/' + original):
    path = content / (relative+'.uasset')
    protected[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
(out / 'protected.json').write_text(json.dumps(protected, indent=2)+'\n')
dest = out / (kind + '.mesh.json')
dest.write_text(json.dumps(data, separators=(',', ':'))+'\n')
audit['output_sha256'] = hashlib.sha256(dest.read_bytes()).hexdigest()
dest.with_suffix('.audit.json').write_text(json.dumps(audit, indent=2)+'\n')
assert all(value == data[key] for key, value in json.loads(raw).items()
    if key not in ('mesh_package', 'skeleton_package'))
print(dest, flush=True)

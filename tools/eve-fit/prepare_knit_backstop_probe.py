"""Prepare a NoSave authoring probe, not production cloth settings."""
import hashlib
import json
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
work = repo/'work/eve26'
output = work/'knit-backstop-probe.json'
hashes = work/'knit-backstop-before.json'
assert not output.exists() and not hashes.exists()
data = json.loads((work/'knit-proxy2.json').read_text())
slot = data['slots']['Collar-1']
influence = slot['authored_maps']['dForce Influence']
slot['max_distances'] = [0. if w <= 1e-7 else 2.*w for w in influence]
slot['backstop_distances'] = [0. if w <= 1e-7 else -.05 for w in influence]
slot['backstop_radii'] = [0. if w <= 1e-7 else 1.+w for w in influence]
assert any(d == 0 for d in slot['max_distances']) and any(d > .1 for d in slot['max_distances'])
data['probe_scope'] = 'Synthetic varied backstop values for NoSave rebuild verification only. No cloth simulation or fitting acceptance.'
output.write_text(json.dumps(data,separators=(',',':'))+'\n')
content = repo.parent/'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSS'
paths = ['EveTest/SK_KFit2.uasset','EveTest/PA_PTail.uasset','Shared/SKEL_Base.uasset',
    'SeduXtress/PA_Body.uasset','SeduXtress/ABP_Secondary.uasset']
hashes.write_text(json.dumps({p:hashlib.sha256((content/p).read_bytes()).hexdigest() for p in paths},indent=2)+'\n')
print(output)

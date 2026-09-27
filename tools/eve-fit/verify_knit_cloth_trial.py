"""Verify fresh private Knitwear cloth binding and all three saved distance maps."""
import hashlib
import json
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
work = repo/'work/eve26'
source = work/'knit-cloth1/readback.json'
output = work/'knit-cloth1/verified.json'
assert not output.exists()
data = json.loads(source.read_text())
assert data['asset'] == '/Game/CSS/EveTest/SK_KCloth1.SK_KCloth1'
assert data['skeleton'] == '/Game/CSS/Shared/SKEL_Base.SKEL_Base'
assert data['mesh_physics'] == '/Game/CSS/SeduXtress/PA_Body.PA_Body'
assert data['post_process'] == '/Game/CSS/EveTest/ABP_KnitFeet1.ABP_KnitFeet1_C'
assert data['cloth_assets'] == len(data['assets']) == len(data['sections']) == 1
asset = data['assets'][0]
section = data['sections'][0]
assert asset['physics'] == '/Game/CSS/EveTest/PA_KCloth1.PA_KCloth1'
assert asset['particles'] == 1679 and asset['iterations'] == 8 and asset['max_iterations'] >= 8
assert asset['legacy_backstop'] is False
expected = json.loads((work/'knit-cloth1/proxy.json').read_text())['slots']['Collar-1']
errors = {}
for name in ('max_distances','backstop_distances','backstop_radii'):
    assert len(asset[name]) == len(expected[name]) == 1679
    errors[name] = max(abs(a-b) for a,b in zip(asset[name],expected[name],strict=True))
    assert errors[name] < 1e-6
assert section['slot'] == 'Collar-1' and section['asset_index'] == 0
assert section['mapping_count'] == section['vertices'] and section['vertices'] > 0
content = repo.parent/'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSS'
protected = json.loads((work/'knit-backstop-before.json').read_text())
assert all(hashlib.sha256((content/p).read_bytes()).hexdigest() == sha for p,sha in protected.items())
output.write_text(json.dumps(dict(readback_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    map_error_cm=errors,render_vertices=section['vertices'],protected_unchanged=True,
    scope='Saved binding and maps only. Simulation, contact, morphs and game remain unverified.'),indent=2)+'\n')
print(output.read_text())

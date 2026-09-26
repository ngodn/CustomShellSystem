"""Verify fresh native readback of the private skeletal tail cloth candidate."""
import hashlib
import json
from pathlib import Path

w=Path(__file__).resolve().parents[2]/'work/eve26'
out=w/'planet-embedded-verified.json'
assert not out.exists()
readback=w/'planet-embedded-native.json'
d=json.loads(readback.read_text())
assert d['skeleton']=='/Game/CSS/Shared/SKEL_Base.SKEL_Base'
assert d['mesh_physics']=='/Game/CSS/SeduXtress/PA_Body.PA_Body'
assert d['post_process']=='/Game/CSS/SeduXtress/ABP_Secondary.ABP_Secondary_C'
assert d['cloth_assets']==len(d['assets'])==len(d['sections'])==1
asset=d['assets'][0];section=d['sections'][0]
assert asset['physics']=='/Game/CSS/EveTest/PA_PTailRear.PA_PTailRear'
assert asset['particles']==18 and asset['iterations']==8 and asset['max_iterations']>=8
expected=json.loads((w/'planet-tail-cloth.json').read_text())['slots']['PlanetTail_17']['max_distances']
assert len(expected)==len(asset['max_distances'])==18
assert max(abs(a-b) for a,b in zip(expected,asset['max_distances'],strict=True))<.00001
assert [i for i,v in enumerate(asset['max_distances']) if v==0]==[0,1,2,3]
assert section['slot']=='PlanetTail_17' and section['asset_index']==0
assert section['vertices']==section['mapping_count']==4916
protected=json.loads((w/'planet-protected-before.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==sha for p,sha in protected.items())
out.write_text(json.dumps({'passed':True,'readback_sha256':hashlib.sha256(readback.read_bytes()).hexdigest(),
    'protected_unchanged':True,'scope':'Fresh saved binding, pin/config/reference verification only; skeletal cloth motion and game behavior pending'},indent=2)+'\n')
print('Embedded Prototype cloth readback passed; production hashes unchanged')

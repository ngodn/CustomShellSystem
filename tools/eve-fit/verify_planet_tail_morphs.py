"""Check saved F14 tail morphs against authored deltas and cloth blend weights."""
import hashlib
import json
from pathlib import Path
import numpy as np

w=Path(__file__).resolve().parents[2]/'work/eve26'
native_path=w/'planet-f14-native.json'
native=json.loads(native_path.read_text())
assert native['asset']=='/Game/CSS/EveTest/SK_PFit14.SK_PFit14'
mesh=json.loads((w/'planet-fit14/planet.mesh.json').read_text())
section=native['render_geometry']['sections'][0]
points=np.asarray(section['positions'])
mapping=np.asarray(section['mapping']).reshape(len(points),-1,9)
blend=(1-mapping[:,:,3]/65535).mean(axis=1)
rows=[]
expected_names={'FBMBodyTone','PBMGlutesSize','PBMHipSize','PBMWaistWidth'}
assert {r['morph'] for r in native['cloth_section_morphs']}==expected_names
for row in native['cloth_section_morphs']:
    assert row['slot']=='PlanetTail_17'
    actual={int(d[0]):np.asarray(d[1:]) for d in row['deltas']}
    assert all(blend[i]==0 for i in actual), 'Saved morph overlaps simulation'
    morph=next(m for m in mesh['morph_targets'] if m['name']==row['morph'])
    authored=[d for d in morph['deltas'] if 65021<=d[0]<65861]
    assert len(authored)==62
    covered=set();errors=[];culled=[]
    for delta in authored:
        matches=np.flatnonzero(np.linalg.norm(points-np.asarray(mesh['points'][delta[0]]),axis=1)<.0005)
        assert len(matches)>0
        for index in matches:
            expected=np.asarray(delta[1:])
            value=actual.get(int(index),np.zeros(3))
            error=float(np.linalg.norm(value-expected))
            if int(index) not in actual:
                # UE5.6 EngineTypes.h defaults MorphThresholdPosition to 0.015 cm.
                assert np.linalg.norm(expected)<.015001, 'Missing significant morph delta'
                culled.append(float(np.linalg.norm(expected)))
            else:
                errors.append(error)
            covered.add(int(index))
    extra=set(actual)-covered
    assert all(np.linalg.norm(actual[i])<1e-8 for i in extra), 'Unexpected saved tail position delta'
    assert max(errors)<.001, (row['morph'],max(errors))
    rows.append(dict(morph=row['morph'],saved_deltas=len(actual),maximum_error_cm=max(errors),
                     culled_deltas=len(culled),maximum_culled_cm=max(culled,default=0),
                     extra_zero_position_records=len(extra)))
out=w/'planet-f14-morph-verified.json'
assert not out.exists()
out.write_text(json.dumps(dict(passed=True,native_sha256=hashlib.sha256(native_path.read_bytes()).hexdigest(),
    morphs=rows,scope='Saved LOD0 morph deltas match authored strap corrections and have zero cloth influence. Runtime rendering and motion remain unverified.'),indent=2)+'\n')
print(rows)

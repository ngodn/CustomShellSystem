"""Preserve fitted garment deformation through independent skirt control bones."""
import copy
import hashlib
import json
from pathlib import Path

work=Path(__file__).resolve().parents[2]/'work/eve26'
source=work/'holiday-hip-clean.mesh.json'
output=work/'holiday-follow.mesh.json'
assert not output.exists()
mesh=json.loads(source.read_text())
before=copy.deepcopy(mesh)
names={b['name']:i for i,b in enumerate(mesh['bones'])}
# These are control carriers, not a new skeleton hierarchy or chain solver.
pairs={
    'pelvis':'F_01','spine_01':'B_01','belly':'F_02',
    'spine_02':'B_02','spine_03':'B_03',
    'butt001':'L_01','butt002':'R_01',
    'thigh_l':'L_02','thigh_r':'R_02',
    'thigh_twist_02_l':'L_03','thigh_twist_02_r':'R_03',
}
mapping={names[s]:names['CSS_Cloth_Skirt_'+t] for s,t in pairs.items()}
slots={'MI_CH_P_EVE_Christmas_01_01.001','MI_CH_P_EVE_Christmas_01_Decal.001',
       'MI_EVE_HR_Christmas_01_Fur.001','MI_EVE_HR_15_Emissive1.001','MI_CH_P_EVE_Christmas_01_03.001'}
ids={mesh['materials'].index(s) for s in slots}
vertices={mesh['wedges'][w][0] for f in mesh['faces'] if f[3] in ids for w in f[:3]}
changed=0
for row in mesh['influences']:
    if row[0] in vertices and row[1] in mapping:
        row[1]=mapping[row[1]];changed+=1
assert changed
assert all(mesh[k]==before[k] for k in mesh if k not in ('influences','mesh_package'))
assert all(a[0]==b[0] and a[2]==b[2] for a,b in zip(mesh['influences'],before['influences'],strict=True))
assert all(a==b for a,b in zip(mesh['influences'],before['influences'],strict=True) if a[0] not in vertices)
mesh['mesh_package']='/Game/CSS/EveTest/SK_HolidayFollow'
output.write_text(json.dumps(mesh,separators=(',',':')))
report={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
        'drivers':{mesh['bones'][v]['name']:mesh['bones'][k]['name'] for k,v in mapping.items()},
        'changed_influences':changed,'garment_vertices':len(vertices),
        'scope':'Offline carrier-bone trial. Requires a rig to copy driver deformation with bind compensation before secondary motion. No skeleton, geometry, weight-value or morph changes. Not deployable alone.'}
(work/'skirt-follow.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

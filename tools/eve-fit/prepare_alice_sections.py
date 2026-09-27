"""Give Alice's ribbon its own visibility section without changing geometry. Python 3.14."""
import copy
import hashlib
import json
from pathlib import Path

work = Path(__file__).resolve().parents[2] / 'work/eve26'
source = work / 'alice-morph1/alice.mesh.json'
data = json.loads(source.read_text())
audit = json.loads(source.with_suffix('.audit.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest() == audit['output_sha256']
result = copy.deepcopy(data)
offset = 0
selected = []
for part in audit['parts']:
    if part['name'] == 'Eve Midsummer Alice - Ribbon':
        selected = list(range(offset, offset + part['faces']))
    offset += part['faces']
assert len(selected) == 1224
assert {data['faces'][i][3] for i in selected} == {16}
assert data['materials'][16] == 'MI_CH_P_EVE_61_Suit'
slot = len(result['materials'])
result['materials'].append('AliceRibbon')
for i in selected:
    result['faces'][i][3] = slot
assert all(result[k] == v for k, v in data.items() if k not in ('faces', 'materials'))
assert all(a[:3] == b[:3] for a, b in zip(data['faces'], result['faces'], strict=True))
result['mesh_package'] = '/Game/CSS/EveTest/SK_AFit1'
result['skeleton_package'] = '/Game/CSS/EveTest/SKEL_AFit1'
audit.update(mesh_package=result['mesh_package'], skeleton_package=result['skeleton_package'])
offset = 0
for part in audit['parts']:
    part['material_slots'] = sorted({f[3] for f in result['faces'][offset:offset + part['faces']]})
    offset += part['faces']
out = work / 'alice-sections1'
out.mkdir(exist_ok=False)
target = out / 'alice.mesh.json'
target.write_text(json.dumps(result, separators=(',', ':')) + '\n')
audit['output_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
target.with_suffix('.audit.json').write_text(json.dumps(audit, indent=2) + '\n')
(out / 'material-aliases.json').write_text(json.dumps({str(slot): {
    'name': 'AliceRibbon', 'source_slot': 16, 'source_material': data['materials'][16]
}}, indent=2) + '\n')
(out / 'receipt.json').write_text(json.dumps({
    'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'ribbon_slot': slot, 'ribbon_faces': len(selected),
    'geometry_weights_morphs_unchanged': True,
    'scope': 'Private section split. Import, material binding and runtime visibility remain unverified.'
}, indent=2) + '\n')
print(target)

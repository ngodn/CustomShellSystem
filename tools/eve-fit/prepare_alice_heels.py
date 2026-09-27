"""Reuse verified Knitwear shoe weights only after exact geometry/rig comparison."""
import copy
import hashlib
import json
from pathlib import Path

work = Path(__file__).resolve().parents[2] / 'work/eve26'
def load(path):
    raw = path.read_bytes()
    audit = json.loads(path.with_suffix('.audit.json').read_text())
    assert hashlib.sha256(raw).hexdigest() == audit['output_sha256']
    start = 0
    for part in audit['parts']:
        if part['name'] == 'Eve Extras - Heels':
            return json.loads(raw), audit, start, part['points']
        start += part['points']
    raise AssertionError('Missing heels')

source = work / 'alice-sections1/alice.mesh.json'
reference = work / 'knit-w2/knit.mesh.json'
data, audit, start, count = load(source)
donor, _, donor_start, donor_count = load(reference)
assert count == donor_count == 31668
assert data['bones'] == donor['bones']
assert data['points'][start:start+count] == donor['points'][donor_start:donor_start+count]
result = copy.deepcopy(data)
local = [[v-donor_start, b, w] for v, b, w in donor['influences'] if donor_start <= v < donor_start+count]
result['influences'] = [r for r in data['influences'] if not start <= r[0] < start+count]
result['influences'] += [[v+start, b, w] for v, b, w in local]
result['influences'].sort(key=lambda r: (r[0], r[1]))
assert all(result[k] == v for k, v in data.items() if k != 'influences')
assert sorted(r for r in result['influences'] if not start <= r[0] < start+count) == sorted(r for r in data['influences'] if not start <= r[0] < start+count)
out = work / 'alice-heels1'
out.mkdir(exist_ok=False)
target = out / 'alice.mesh.json'
target.write_text(json.dumps(result, separators=(',', ':')) + '\n')
audit['output_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
next(p for p in audit['parts'] if p['name'] == 'Eve Extras - Heels')['max_influences'] = 8
target.with_suffix('.audit.json').write_text(json.dumps(audit, indent=2) + '\n')
(out / 'receipt.json').write_text(json.dumps({
    'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'reference_sha256': hashlib.sha256(reference.read_bytes()).hexdigest(),
    'shoe_points': count, 'replacement_weight_rows': len(local),
    'geometry_and_bones_match': True, 'body_geometry_and_morphs_unchanged': True,
    'scope': 'Shoe weight reuse; needs corrective foot graph and game validation.'
}, indent=2) + '\n')
print(target)

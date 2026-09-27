"""Replace only private proxy normals using verified render correspondence."""
import copy
import hashlib
import json
from pathlib import Path
import numpy as np

work = Path(__file__).resolve().parents[2] / 'work/eve26'
output = work / 'knit-proxy2.json'
receipt = work / 'knit-proxy2-repair.json'
assert not output.exists() and not receipt.exists()
source = work / 'knit-proxy1.json'
audit_path = work / 'knit-normals1.json'
original = json.loads(source.read_text())
audit = json.loads(audit_path.read_text())
assert original['report']['source_sha256'] == audit['source_sha256']
result = copy.deepcopy(original)
normals = audit['transferred_normals']
assert len(normals) == len(result['slots']['Collar-1']['positions'])
assert np.isfinite(normals).all() and np.allclose(np.linalg.norm(normals, axis=1), 1)
result['slots']['Collar-1']['normals'] = normals
output.write_text(json.dumps(result, separators=(',', ':'))+'\n')
reloaded = json.loads(output.read_text())
assert reloaded['slots']['Collar-1']['normals'] == normals
reloaded['slots']['Collar-1']['normals'] = original['slots']['Collar-1']['normals']
assert reloaded == original
receipt.write_text(json.dumps(dict(
    input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    audit_sha256=hashlib.sha256(audit_path.read_bytes()).hexdigest(),
    output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
    changed_field='slots.Collar-1.normals', all_other_fields_unchanged=True,
    scope='Private proxy only. No native binding or motion acceptance.'), indent=2)+'\n')
print(receipt.read_text())

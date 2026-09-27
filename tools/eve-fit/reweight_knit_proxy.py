"""Transfer W2 garment weights through the existing cloth proxy correspondence."""
import copy
import hashlib
import json
from pathlib import Path

work = Path(__file__).resolve().parents[2]/'work/eve26'
source = work/'knit-proxy3.json'
candidate = work/'knit-w2/knit.mesh.json'
output = work/'knit-w2/proxy.json'
assert not output.exists()
proxy = json.loads(source.read_text())
original = copy.deepcopy(proxy)
mesh = json.loads(candidate.read_text())
audit = json.loads(candidate.with_suffix('.audit.json').read_text())
assert hashlib.sha256(candidate.read_bytes()).hexdigest() == audit['output_sha256']
assert audit['parts'][1]['name'] == 'Eve Extras - Sweater'
start, count = audit['parts'][0]['points'], audit['parts'][1]['points']
weights = [{} for _ in range(count)]
for vertex, bone, weight in mesh['influences']:
    if start <= vertex < start+count:
        weights[vertex-start][mesh['bones'][bone]['name']] = weight
slot = proxy['slots']['Collar-1']
changed, dropped = 0, 0.
for i, transfer in enumerate(slot['transfer']):
    combined = {}
    for vertex, alpha in zip(transfer['vertices'], transfer['barycentric'], strict=True):
        for bone, weight in weights[vertex].items():
            combined[bone] = combined.get(bone, 0)+alpha*weight
    assert abs(sum(combined.values())-1) < 1e-5
    kept = sorted(((b,w) for b,w in combined.items() if w > 1e-8), key=lambda x:-x[1])[:8]
    total = sum(w for _,w in kept)
    dropped = max(dropped, 1-total)
    rows = [[b,w/total] for b,w in kept]
    previous = dict(slot['weights'][i])
    changed += any(abs(previous.get(b,0)-w) > 1e-6 for b,w in rows)
    slot['weights'][i] = rows
assert all(slot[k] == value for k,value in original['slots']['Collar-1'].items() if k != 'weights')
proxy['report']['source_sha256'] = audit['output_sha256']
proxy['weight_source'] = str(candidate)
output.write_text(json.dumps(proxy,separators=(',',':'))+'\n')
receipt = dict(changed_proxy_vertices=changed,max_dropped_weight=dropped,
    proxy_geometry_maps_and_correspondence_unchanged=True,
    scope='W2 weights transferred to the same proxy, at most eight influences. No native binding or motion acceptance.')
(work/'knit-w2/proxy-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt)

"""Give each rigid hip fastener a shared attachment blend without moving geometry."""
import collections
import copy
import hashlib
import json
from pathlib import Path

w = Path(__file__).resolve().parents[2]/'work/eve26'
p = w/'planet-neck-trial/planet.mesh.json'
raw = p.read_bytes()
m = json.loads(raw)
audit = json.loads(p.with_name('planet.mesh.audit.json').read_text())
assert hashlib.sha256(raw).hexdigest() == audit['output_sha256']
start = audit['parts'][0]['faces']
adj = collections.defaultdict(set)
for f in m['faces'][start:start+audit['parts'][1]['faces']]:
    ids = [m['wedges'][i][0] for i in f[:3]]
    for v in ids:
        adj[v].update(ids)
components = []
for seed in [52822, 52966]:
    todo, ids = [seed], set()
    while todo:
        v = todo.pop()
        if v in ids:
            continue
        ids.add(v)
        todo.extend(adj[v]-ids)
    assert len(ids) == 409
    components.append(ids)
assert not components[0] & components[1]
changed = set.union(*components)
result = copy.deepcopy(m)
result['influences'] = [row for row in m['influences'] if row[0] not in changed]
reports = []
for ids in components:
    sums = collections.Counter()
    for v, bone, weight in m['influences']:
        if v in ids:
            sums[bone] += weight
    total = sum(sums.values())
    weights = {b: value/total for b, value in sums.items()}
    assert len(weights) <= 8 and abs(sum(weights.values())-1) < 1e-8
    for v in sorted(ids):
        result['influences'].extend([v, b, weight] for b, weight in sorted(weights.items()))
    reports.append(dict(vertices=sorted(ids), shared_weights={m['bones'][b]['name']: weight for b, weight in weights.items()}))
assert all(result[k] == value for k, value in m.items() if k != 'influences')
out = w/'planet-fastener-trial'
out.mkdir(exist_ok=False)
target = out/'planet.mesh.json'
target.write_text(json.dumps(result, separators=(',', ':'))+'\n')
audit['output_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
(out/'planet.mesh.audit.json').write_text(json.dumps(audit, indent=2)+'\n')
(out/'receipt.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(raw).hexdigest(),
    components=reports, geometry_and_morphs_unchanged=True,
    scope='Uniform existing mean attachment blend per fastener. No new bones or point changes. Trial requires motion, contact and visual comparison.'), indent=2)+'\n')
print('Prepared two fasteners,', len(changed), 'vertices; no geometry or morph changes')

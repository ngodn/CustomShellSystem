"""Reuse weights only when named parts have identical geometry and topology."""
import argparse
import copy
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
p.add_argument('--target', type=Path, required=True)
p.add_argument('--part', required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--stem', default='knit', choices=('knit',))
a = p.parse_args()
assert not a.output.exists()

def read(path):
    raw = path.read_bytes()
    data = json.loads(raw)
    audit = json.loads(path.with_suffix('.audit.json').read_text())
    assert hashlib.sha256(raw).hexdigest() == audit['output_sha256']
    index = next(i for i, part in enumerate(audit['parts']) if part['name'] == a.part)
    start = sum(part['points'] for part in audit['parts'][:index])
    count = audit['parts'][index]['points']
    first = sum(part['faces'] for part in audit['parts'][:index])
    faces = data['faces'][first:first + audit['parts'][index]['faces']]
    topology = [([data['wedges'][w][0] - start for w in f[:3]], data['materials'][f[3]]) for f in faces]
    return data, audit, start, count, topology

source, sa, ss, sn, sf = read(a.source)
target, ta, ts, tn, tf = read(a.target)
assert sn == tn and sf == tf, 'Part topology or material assignment differs'
assert source['points'][ss:ss+sn] == target['points'][ts:ts+tn], 'Part geometry differs'
assert source['bones'] == target['bones'], 'Bone indices or bind transforms differ'
result = copy.deepcopy(target)
replacement = [[v-ss+ts, b, w] for v, b, w in source['influences'] if ss <= v < ss+sn]
assert {v for v, _, _ in replacement} == set(range(ts, ts+tn))
result['influences'] = [row for row in target['influences'] if not ts <= row[0] < ts+tn] + replacement
assert all(result[k] == value for k, value in target.items() if k != 'influences')
def maps(rows):
    out = {}
    for v, b, w in rows:
        out.setdefault(v, {})[b] = w
    return out
before, after = maps(target['influences']), maps(result['influences'])
changed = [v for v in before if before[v] != after[v]]
assert all(ts <= v < ts+tn for v in changed)
a.output.mkdir()
path = a.output / f'{a.stem}.mesh.json'
path.write_text(json.dumps(result, separators=(',', ':')) + '\n')
target_hash = ta['output_sha256']
ta['output_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
path.with_suffix('.audit.json').write_text(json.dumps(ta, indent=2) + '\n')
receipt = dict(source_sha256=sa['output_sha256'], target_sha256=target_hash,
    part=a.part, points=tn, changed_vertices=len(changed),
    identical_geometry_topology_and_bones=True, other_parts_and_fields_unchanged=True,
    scope='Weight reuse only. Requires pose, source-library and runtime verification.')
(a.output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(receipt)

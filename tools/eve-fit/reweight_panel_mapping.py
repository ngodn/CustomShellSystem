"""Prepare an offline nearest-support diagnostic, never a native cloth asset."""
import argparse
import hashlib
import json
import math
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--mapping', type=Path, required=True)
p.add_argument('--proxy', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
assert not a.output.exists()
mapping = json.loads(a.mapping.read_text())
proxy = json.loads(a.proxy.read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
section = mapping['render_geometry']['sections'][1]
assert 'XM_Dress01' in mapping['sections'][1]['material']
records = section['mapping']
n = len(records)//len(section['positions'])
assert n == 5 and len(records) == n*len(section['positions'])
changed = 0
for start in range(0, len(records), n):
    batch = records[start:start+n]
    active = [i for i, row in enumerate(batch) if row[3] < 65535 and row[8] > 0]
    if len(active) < 2:
        continue
    def score(i):
        row = batch[i]
        xyz = [proxy['positions'][int(v)] for v in row[:3]]
        span = max(math.dist(xyz[j], xyz[(j+1)%3]) for j in range(3))
        bary = [row[4], row[5], 1-row[4]-row[5]]
        return abs(row[7])+span*max(0., sum(abs(x) for x in bary)-1)
    chosen = min(active, key=score)
    for i, row in enumerate(batch):
        row[8] = 1. if i == chosen else 0.
    changed += 1
mapping['diagnostic_mapping'] = {
    'scope': 'Offline position replay only. Chooses one existing accurate support per dynamic main-fabric vertex by normal offset plus extrapolation distance. Does not change simulation or author a native asset; continuity and normal/tangent behavior unverified.',
    'source': str(a.mapping), 'source_sha256': hashlib.sha256(a.mapping.read_bytes()).hexdigest(),
    'proxy_sha256': hashlib.sha256(a.proxy.read_bytes()).hexdigest(), 'changed_vertices': changed}
a.output.write_text(json.dumps(mapping, separators=(',', ':'))+'\n')
print(mapping['diagnostic_mapping'])

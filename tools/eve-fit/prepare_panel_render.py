"""Prepare verified Chaos particle output for the existing diagnostic renderer."""
import argparse
import hashlib
import json
import math
from pathlib import Path

work = Path(__file__).resolve().parents[2]/'work/eve26'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--verification', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
assert not a.output.exists()
verification = json.loads(a.verification.read_text())
assert verification['passed']
assert verification['source_sha256'] == hashlib.sha256(a.input.read_bytes()).hexdigest()
result = json.loads(a.input.read_text())
mesh = json.loads((work/'holiday.mesh.json').read_text())
proxy = json.loads((work/'skirt-proxies.json').read_text())['slots']['MI_CH_P_EVE_Christmas_01_01.001']
ids = json.loads((work/'skirt-f11.json').read_text())['source_vertices']
assert len(ids) == len(proxy['positions'])
assert max(math.dist(mesh['points'][v], p) for v, p in zip(ids, proxy['positions'], strict=True)) < 1e-5
assert all(len(frame['positions_cm']) == len(ids) for frame in result['frames'])
a.output.write_text(json.dumps({'scope':result['scope']+' Main surface only; the engine render attachment mappings are not replayed by this renderer.',
    'source_motion':result['source_motion'],'morph_case':'default','source_vertices':ids,
    'frames':result['frames']},separators=(',',':')))
print('Prepared',len(result['frames']),'frames from verified native output')

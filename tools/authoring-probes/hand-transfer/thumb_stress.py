"""Reproduce the six retained thumb contact failures on actual V44B2 skin."""
import json
import math
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, load, pair_set

OUT = Path(os.environ['CSS_THUMB_STRESS_AUDIT_DIR']).resolve()
assert OUT.parent == WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT / 'report.json').exists()
skin = SkinFixture()
controls = AUDIT / 'left-corrected-controller-sweep-v1'
neutral = skin.evaluate(load(controls / 'control-0.json'))
neutral_pairs = pair_set(neutral)
assert len(neutral_pairs) == 4
(OUT / 'neutral-contacts.json').write_text(json.dumps(neutral, indent=2) + '\n')
prior = load(AUDIT / 'left-graph-anchor-thumb-stress-v1-contacts/report.json')['samples']
cases = [r for r in prior if r['new_triangle_pairs']]
assert len(cases) == 6
rows = []
started = time.monotonic()
for case in cases:
    name = case['pose']
    doc = load(AUDIT / 'left-graph-anchor-thumb-stress-v1' / name)
    values = skin.shapes(doc)
    expected = load(AUDIT / 'left-graph-anchor-thumb-stress-v1-contacts' / ('shapes-' + name))['values']
    error = max(abs(values[n] - expected[n]) for n in values)
    assert error < 1e-5, (name, error)
    result = skin.evaluate(doc, values)
    extra = pair_set(result) - neutral_pairs
    angles = {m['shape']: [math.degrees(v) for v in skin.source_rotations(doc)[m['shape']].to_euler(m['euler_order'])]
              for m in skin.curves if m['bone'].startswith('thumb')}
    row = dict(pose=name, new_pairs=len(extra), regions=result['regions'],
               prior_new_pairs=len(case['new_triangle_pairs']), corrective_error=error,
               source_thumb_euler_degrees=angles)
    (OUT / ('contacts-' + name)).write_text(json.dumps(result, indent=2) + '\n')
    rows.append(row)
    print(json.dumps(row), flush=True)
skin.assert_unchanged()
(OUT / 'report.json').write_text(json.dumps(dict(scope=__doc__, blend_sha256=skin.source_hash,
    samples=rows, failures=sum(r['new_pairs'] > 0 for r in rows), seconds=time.monotonic()-started), indent=2) + '\n')
raise SystemExit(2 if any(r['new_pairs'] for r in rows) else 0)

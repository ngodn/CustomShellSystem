"""Verify the clip response comparison and its isolated movement-limit intervention."""
import hashlib
import json
import math
from pathlib import Path
import sys


stage = Path(sys.argv[1])
data = json.loads((stage/'response.json').read_text())
assert json.loads((stage/'exit.json').read_text())['returncode'] == 0
assert data['inputs_unchanged']
expected = {(clip['package'], mode, preset) for clip in data['clips']
            for mode in ('current', 'current_old_limits', 'v42')
            for preset in ('firm', 'natural', 'off')}
assert len(expected) == len(data['cases']) == 18
seen, rows = set(), {}
for row in data['cases']:
    identity = row['clip'], row['mode'], row['preset']
    assert identity not in seen
    seen.add(identity)
    clip = next(c for c in data['clips'] if c['package'] == row['clip'])
    assert clip['mapped_bones'] == 379
    assert all(not values for values in clip['unmapped_rig_bones'].values())
    assert len(row['samples_degrees']) == clip['keys'] == len(row['timing'])
    assert all(math.isfinite(v) for sample in row['samples_degrees'] for v in sample)
    assert row['timing'][-1]['resets'] == 1
    if row['preset'] == 'off':
        assert max(row['peak_degrees']) < .001
    rows[identity] = row
assert seen == expected
iris = '/Game/CSS/Locomotion/Studies/L5X/AN_IrisOut'
assert max(rows[iris, 'current', 'firm']['peak_degrees'][:2]) > 15
for preset in ('firm', 'natural'):
    fixed = rows[iris, 'current_old_limits', preset]['peak_degrees']
    assert max(fixed[:2]) < 4 and max(fixed[2:4]) < 8.1
    current = rows[iris, 'current', preset]['tuning']
    limited = rows[iris, 'current_old_limits', preset]['tuning']
    assert {name for name in current if current[name] != limited[name]} == {'Limits', 'MaxTravel'}
result = dict(cases=18, mapped_bones=379, only_initialization_reset=True,
              disabled_relative_motion_zero=True, isolated_limits_reduce_large_motion=True,
              response_sha256=hashlib.sha256((stage/'response.json').read_bytes()).hexdigest(),
              results=[dict(clip=row['clip'], mode=row['mode'], preset=row['preset'],
                            peak_degrees=row['peak_degrees']) for row in data['cases']])
(stage/'verified.json').write_text(json.dumps(result, indent=2)+'\n')
print('18 cases verified; limit-only intervention reduces large-motion response')

"""Compare the freshly exported source candidate with the visually checked hip repair."""
import json
import math
from pathlib import Path

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
original = json.loads((WORK/'holiday.mesh.json').read_text())
expected = json.loads((WORK/'holiday-hip.mesh.json').read_text())
actual = json.loads((WORK/'holiday-hip-source.mesh.json').read_text())
for key in ('bones', 'materials', 'uv_channels', 'wedges', 'faces', 'colors'):
    assert actual[key] == original[key], key
assert len(actual['points']) == len(expected['points'])
error = max(math.dist(a, b) for a, b in zip(actual['points'], expected['points']))
assert error < .0005, error
body_count = json.loads((WORK/'holiday.mesh.audit.json').read_text())['parts'][0]['points']
assert actual['points'][:body_count] == original['points'][:body_count], 'Body points changed'
assert len(actual['morph_targets']) == len(original['morph_targets']) == 22
morph_error = 0.
for a, b in zip(actual['morph_targets'], original['morph_targets']):
    assert a['name'] == b['name'] and len(a['deltas']) == len(b['deltas']), a['name']
    for u, v in zip(a['deltas'], b['deltas']):
        assert u[0] == v[0], a['name']
        morph_error = max(morph_error, math.dist(u[1:], v[1:]))
assert morph_error < .0005, morph_error
print('Geometry and morph checks passed', error, morph_error)
assert actual['influences'] == original['influences'], 'Weights differ; source audit required'
report = {'max_point_error_cm': error, 'max_relative_morph_error_cm': morph_error,
          'body_points_unchanged': True, 'morphs': 22,
          'scope': 'Fresh source export matches the visually checked local correction; no gameplay or full outfit acceptance.'}
(WORK/'hip-source-verified.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))

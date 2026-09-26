"""Measure repeated native cloth runs without attributing their differences to a cause."""
import argparse
import hashlib
import json
import math
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline', type=Path, required=True)
p.add_argument('--repeat', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
assert not a.output.exists()
left = json.loads(a.baseline.read_text())
right = json.loads(a.repeat.read_text())
assert {k: v for k, v in left.items() if k != 'frames'} == {
    k: v for k, v in right.items() if k != 'frames'}
rows = []
arrays = ('positions_cm', 'normals', 'body_reference_skin_cm')
for f, g in zip(left['frames'], right['frames'], strict=True):
    assert {k: v for k, v in f.items() if k not in (*arrays, 'simulation_ms')} == {
        k: v for k, v in g.items() if k not in (*arrays, 'simulation_ms')}
    row = {'frame': f['frame']}
    for key in arrays:
        distances = [math.dist(x, y) for x, y in zip(f[key], g[key], strict=True)]
        assert distances and all(math.isfinite(x) for x in distances)
        row[key] = {'max_distance': max(distances),
                    'rms_distance': math.sqrt(sum(x*x for x in distances)/len(distances))}
    rows.append(row)
report = {
    'scope': 'Two native runs with identical reported metadata and frame settings. '
             'Variation is measured, not attributed to solver ordering, cache, or another cause. '
             'This does not establish statistical equivalence of another collider.',
    'inputs': {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
               for path in (a.baseline, a.repeat)},
    'metadata_and_frame_settings_identical': True,
    'maxima': {key: max(row[key]['max_distance'] for row in rows) for key in arrays},
    'frames': rows,
}
a.output.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report['maxima']))

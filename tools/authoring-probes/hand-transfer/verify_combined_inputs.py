"""Verify reconstructed original inputs against retained calibration evidence."""
import hashlib
import json
import math
import os
import sys
from pathlib import Path
from mathutils import Quaternion

sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import AUDIT, WORK, HERE, load
from calibration import apply

out = Path(os.environ['CSS_COMBINED_HAND_FIXTURES']).resolve()
assert out.parent == WORK.resolve()
assert not (out/'input-reconstruction.json').exists()
fixture_path = out/'fixtures.json'
fixtures = load(fixture_path)
calibration_path = HERE/'left-finger-calibration-v1.json'
models = load(calibration_path)['parameters']
older_path = WORK/'hand-native-calibration-fixtures-v2/fixtures.json'
older = {c['label']:c for c in load(older_path)['cases']}
digest = lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sources = {str(older_path):digest(older_path)}
rows = []
for case in fixtures['cases']:
    path = Path(case['source']);assert digest(path) == case['source_sha256']
    doc = load(path)
    result,detail = apply(doc,models,case['v43_compatible'])
    assert detail['valid'] != case['singular'],case['label']
    if case['singular']:
        assert doc == result
        rows.append(dict(label=case['label'],valid=False,max_error_degrees=0))
        continue
    if case['label'].startswith('synthetic-'):
        expected_path = AUDIT/'left-graph-anchor-animation-stress-v2'/case['label'].removeprefix('synthetic-')
        sources[str(expected_path)] = digest(expected_path)
        expected = load(expected_path)['pose']['Snapshot']['LocalTransforms']
        wanted = {m['bone']:[expected[m['index']]['Rotation'][k] for k in 'WXYZ'] for m in models}
    else:
        wanted = older[case['label'].removeprefix('calibration-')]['expected_wxyz']
    errors = []
    for m in models:
        q = Quaternion([result['pose']['Snapshot']['LocalTransforms'][m['index']]['Rotation'][k] for k in 'WXYZ']).normalized()
        d = q.rotation_difference(Quaternion(wanted[m['bone']]).normalized())
        errors.append(math.degrees(2*math.atan2(math.sqrt(d.x*d.x+d.y*d.y+d.z*d.z),abs(d.w))))
    rows.append(dict(label=case['label'],valid=True,max_error_degrees=max(errors)))
maximum = max(r['max_error_degrees'] for r in rows)
report = dict(scope=__doc__,cases=rows,fixture_sha256=digest(fixture_path),
              calibration_sha256=digest(calibration_path),reference_sources=sources,
              max_error_degrees=maximum,passed=maximum<.001)
(out/'input-reconstruction.json').write_text(json.dumps(report,indent=2)+'\n')
print('Input reconstruction',len(rows),'cases, maximum degrees',maximum,flush=True)
raise SystemExit(0 if report['passed'] else 2)

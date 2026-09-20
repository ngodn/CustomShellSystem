"""Evaluate complete native hand rotations and curves on original fitted B2 skin."""
import hashlib
import json
import os
import sys
import time
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, load, pair_set

out = Path(os.environ['CSS_COMBINED_HAND_SKIN_DIR']).resolve()
native = Path(os.environ['CSS_COMBINED_HAND_DIR']).resolve()
assert out.parent == native.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'report.json').exists()
digest = lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report = load(native/'report.json')
assert report['passed'] and load(native/'exit.json')['exit_code'] == 0
fixture_path = Path(report['fixture_path'])
assert digest(fixture_path) == report['fixture_sha256']
manifest = load(fixture_path)
fixtures = manifest['cases']
if manifest.get('sampling') == 'original-input-running-transitions-v1':
    assert len(fixtures) == len(report['cases']) == 759
    assert sum(c['fraction']>0 for c in fixtures) == 423
else:
    assert len(fixtures) == 454 and len(report['cases']) == 914
measured = {r['label']:r for r in report['cases'] if r['enabled'] and not r['antipodes']}
skin = SkinFixture()
neutral = pair_set(skin.evaluate(load(AUDIT/'left-corrected-controller-sweep-v1/control-0.json'),batch_pose=True))
assert len(neutral) == 4
rows = []; skipped = []; started = time.monotonic()
for i,case in enumerate(fixtures):
    row = measured[case['label']]
    if case['singular']:
        assert not row['valid']
        skipped.append(dict(label=case['label'],reason='Singular input must pass through; deformation quality is not an acceptance target.'))
        continue
    assert row['valid']
    path = Path(case['source']); assert digest(path) == case['source_sha256']
    doc = load(path)
    for name,q in row['output_rotations_xyzw'].items():
        doc['pose']['Snapshot']['LocalTransforms'][skin.index[name]]['Rotation'] = dict(zip('XYZW',q))
    shapes = {m['shape']:max(0,min(1,row['curves'][m['shape']]+m['baked_value'])) for m in skin.curves}
    contact = skin.evaluate(doc,shapes,batch_pose=True)
    extra = pair_set(contact)-neutral
    rows.append(dict(label=case['label'],new_pairs=len(extra),total_pairs=contact['crossing_pairs']))
    (out/(case['label']+'.json')).write_text(json.dumps(doc)+'\n')
    (out/('shapes-'+case['label']+'.json')).write_text(json.dumps(dict(values=shapes))+'\n')
    if extra:(out/('contacts-'+case['label']+'.json')).write_text(json.dumps(contact,indent=2)+'\n')
    if extra or i%50 == 0:print(json.dumps(dict(progress=i+1,**rows[-1])),flush=True)
skin.assert_unchanged()
passed = not any(r['new_pairs'] for r in rows)
result = dict(scope=__doc__,passed=passed,samples=rows,skipped=skipped,
              failures=sum(bool(r['new_pairs']) for r in rows),
              native_report_sha256=digest(native/'report.json'),seconds=time.monotonic()-started)
(out/'report.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('samples','skipped')}),flush=True)
raise SystemExit(0 if passed else 2)

"""Paired actual-skin checks of the experimental web guard on V44B2."""
import json
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, HERE, load, pair_set
from thumb_web_guard import apply

out = Path(os.environ['CSS_THUMB_WEB_AUDIT_DIR']).resolve()
assert out.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out / 'report.json').exists()
assert load(WORK / 'arm-rest-batch-pose-v1/report.json')['passed']
model_path = (HERE / os.environ.get('CSS_THUMB_WEB_MODEL', 'thumb-web-guard-v1.json')).resolve()
assert model_path.parent == HERE.resolve()
model = load(model_path)
skin = SkinFixture()
driver = next(m for m in skin.curves if m['bone'] == model['bone'])
neutral = pair_set(skin.evaluate(load(AUDIT / 'left-corrected-controller-sweep-v1/control-0.json'), batch_pose=True))
assert len(neutral) == 4
cases = []
for r in load(AUDIT / 'left-graph-anchor-thumb-stress-v1/manifest.json')['cases']:
    cases.append(dict(group='synthetic', label=r['pose'], source=AUDIT/'left-graph-anchor-thumb-stress-v1'/r['pose']))
for r in load(AUDIT / 'left-observed-thumb-clearance-v1/manifest.json')['cases']:
    cases.append(dict(group='captured', label=r['pose'], source=AUDIT/'left-observed-thumb-clearance-v1'/r['pose']))
for degrees in range(0, 61, 5):
    cases.append(dict(group='control', label=f'control-{degrees}.json', source=AUDIT/f'left-corrected-controller-sweep-v1/control-{degrees}.json'))
for sample in range(5):
    cases.append(dict(group='h2', label=f'h2-{sample}.json', source=WORK/f'arm-rest-hand-transfer-v1/all-h2-samples-v2/h2-s{sample}-calibrated.json'))
assert len(cases) == 464
rows = []
started = time.monotonic()
for i, case in enumerate(cases):
    doc = load(case['source'])
    candidate, correction = apply(doc, model, driver)
    before = doc['pose']['Snapshot']['LocalTransforms']
    after = candidate['pose']['Snapshot']['LocalTransforms']
    assert all(a == b for n, (a,b) in enumerate(zip(before,after,strict=True)) if n != driver['index'])
    assert all(a[k] == b[k] for a,b in zip(before,after,strict=True) for k in ('Translation','Scale3D'))
    base_result = skin.evaluate(doc, batch_pose=True)
    base_extra = pair_set(base_result)-neutral
    if correction['correction_degrees']:
        result = skin.evaluate(candidate, batch_pose=True)
        (out / (case['group']+'-'+case['label'])).write_text(json.dumps(candidate,indent=2)+'\n')
        (out / ('shapes-'+case['group']+'-'+case['label'])).write_text(json.dumps(dict(values=skin.shapes(candidate)),indent=2)+'\n')
    else:
        result = base_result
    extra = pair_set(result)-neutral
    row = dict(group=case['group'], label=case['label'], source=str(case['source']),
               before_new_pairs=len(base_extra), new_pairs=len(extra), regions=result['regions'], **correction)
    rows.append(row)
    if extra:
        (out / ('contacts-'+case['group']+'-'+case['label'])).write_text(json.dumps(result,indent=2)+'\n')
    if correction['correction_degrees'] or extra or i % 50 == 0:
        print(json.dumps(dict(progress=i+1,**row)),flush=True)
skin.assert_unchanged()
groups = {g:dict(total=sum(r['group']==g for r in rows),
                 before_failures=sum(r['group']==g and r['before_new_pairs']>0 for r in rows),
                 failures=sum(r['group']==g and r['new_pairs']>0 for r in rows),
                 changed=sum(r['group']==g and r['correction_degrees']>0 for r in rows)) for g in ('synthetic','captured','control','h2')}
passed = not any(r['new_pairs'] or r['correction_saturated'] or
                 (r['group']=='control' and r['correction_degrees']) for r in rows)
(out / 'report.json').write_text(json.dumps(dict(scope=__doc__, model=model, samples=rows,
    groups=groups, passed=passed, seconds=time.monotonic()-started),indent=2)+'\n')
print(json.dumps(dict(passed=passed,groups=groups,seconds=time.monotonic()-started)),flush=True)
raise SystemExit(0 if passed else 2)

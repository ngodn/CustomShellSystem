"""Evaluate measured native web rotations and curves through actual B2 skin."""
import copy
import hashlib
import json
import os
import sys
import time
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, HERE, load, pair_set

out=Path(os.environ['CSS_NATIVE_WEB_SKIN_DIR']).resolve()
assert out.parent==WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'report.json').exists()
fixture_path=WORK/'hand-native-web-fixtures-v1/fixtures.json'
native_path=WORK/'hand-native-web-rig-v2/report.json'
fixtures=load(fixture_path)['cases']
native=load(native_path)
assert native['passed']
assert native['fixture_sha256']==hashlib.sha256(fixture_path.read_bytes()).hexdigest()
assert load(WORK/'hand-native-web-rig-v2/result.json')['exit_code']==0
results={r['label']:r for r in native['cases'] if r['enabled'] and not r['antipodes']}
skin=SkinFixture()
neutral=pair_set(skin.evaluate(load(AUDIT/'left-corrected-controller-sweep-v1/control-0.json'),batch_pose=True))
rows=[]
started=time.monotonic()
for i,case in enumerate(fixtures):
    measured=results[case['label']]
    doc=dict(pose=dict(Snapshot=copy.deepcopy(case['snapshot'])))
    doc['pose']['Snapshot']['LocalTransforms'][skin.index['thumb_01_l']]['Rotation']=dict(zip('XYZW',measured['output_rotation_xyzw']))
    shapes={m['shape']:max(0,min(1,measured['curves'][m['shape']]+m['baked_value'])) for m in skin.curves}
    contact=skin.evaluate(doc,shapes,batch_pose=True)
    extra=pair_set(contact)-neutral
    row=dict(label=case['label'],new_pairs=len(extra),total_pairs=contact['crossing_pairs'],regions=contact['regions'])
    rows.append(row)
    if extra:
        (out/('contacts-'+case['label']+'.json')).write_text(json.dumps(contact,indent=2)+'\n')
    if case['label']=='synthetic-running_attack-5-alpha-1.0.json':
        (out/'native-pose.json').write_text(json.dumps(doc,indent=2)+'\n')
        (out/'native-shapes.json').write_text(json.dumps(dict(values=shapes),indent=2)+'\n')
    if extra or i%100==0: print(json.dumps(dict(progress=i+1,**row)),flush=True)
skin.assert_unchanged()
passed=all(r['new_pairs']==0 and r['total_pairs']==4 for r in rows)
(out/'report.json').write_text(json.dumps(dict(scope=__doc__,passed=passed,samples=rows,
    native_report_sha256=hashlib.sha256(native_path.read_bytes()).hexdigest(),seconds=time.monotonic()-started),indent=2)+'\n')
print(json.dumps(dict(passed=passed,samples=len(rows),seconds=time.monotonic()-started)),flush=True)
raise SystemExit(0 if passed else 2)

"""Original source inputs for the complete native V44 hand graph.

Synthetic overlays retain the saved captured body/wrist. They do not represent
the full game animation graph or whole-body motion of the decoded source clip.
"""
import copy
import hashlib
import json
import os
import sys
from pathlib import Path
from mathutils import Quaternion

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import AUDIT, WORK, HERE, load

out = Path(os.environ['CSS_COMBINED_HAND_FIXTURES']).resolve()
assert out.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'fixtures.json').exists()
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
models = load(HERE/'left-finger-calibration-v1.json')['parameters']
sources = {}
def read(path):
    sources[str(path)] = digest(path)
    return load(path)

overlay = read(WORK/'genessa-hand-overlay-r2/additive-deltas.json')
assert overlay['poseKind'] == 'local-space-additive-deltas'
delta = {n.lower(): Quaternion([t['Rotation'][k] for k in 'WXYZ']).normalized()
         for n,t in overlay['rows'][0]['deltas'].items()}
paths = {'shared_idle': WORK/'active-idle-exact-v1/animation-poses.json',
         'running_attack': WORK/'running-attack-dense-hand-v2/dense/animation-poses.json'}
paths.update({p.parent.name:p for p in (WORK/'scythe-animation-matrix-exact-v1').glob('*/animation-poses.json')
              if p.parent.name != 'running_attack'})
clips = {name:read(path) for name,path in paths.items()}
assert all(c['retargetSource'] == 'SK_DarkForm' for c in clips.values())
template = read(WORK/'post-process-hand-isolation/bypassed-0.json')
manifest = read(AUDIT/'left-graph-anchor-animation-stress-v2/manifest.json')
cases = []
for case in manifest['cases']:
    clip = clips[case['animation']]
    frame = next(f for f in clip['frames'] if f['frame'] == case['frame'])
    lookup = {n.lower():i for i,n in enumerate(frame['BoneNames'])}
    doc = copy.deepcopy(template)
    for m in models:
        raw = Quaternion([frame['LocalTransforms'][lookup[m['bone']]]['Rotation'][k] for k in 'WXYZ']).normalized()
        additive = Quaternion((1,0,0,0)).slerp(delta[m['bone']],case['overlay_alpha'])
        q = (additive @ raw).normalized()
        doc['pose']['Snapshot']['LocalTransforms'][m['index']]['Rotation'] = dict(zip('XYZW',(q.x,q.y,q.z,q.w)))
    path = out/('synthetic-'+case['pose'])
    path.write_text(json.dumps(doc)+'\n')
    cases.append(dict(label='synthetic-'+case['pose'],source=str(path),source_sha256=digest(path),
                      v43_compatible=False,singular=False))
assert len(cases) == 411
fixture = read(WORK/'hand-native-calibration-fixtures-v2/fixtures.json')
assert len(fixture['cases']) == 43
for case in fixture['cases']:
    doc = read(Path(case['source']))
    for m in models:
        q = case['inputs_wxyz'][m['bone']]
        doc['pose']['Snapshot']['LocalTransforms'][m['index']]['Rotation'] = dict(zip('XYZW',q[1:]+q[:1]))
    path = out/('calibration-'+case['label']+'.json')
    path.write_text(json.dumps(doc)+'\n')
    cases.append(dict(label='calibration-'+case['label'],source=str(path),source_sha256=digest(path),
                      v43_compatible=case['v43_compatible'],singular=case['singular']))
assert len(cases) == 454
(out/'fixtures.json').write_text(json.dumps(dict(scope=__doc__,sources=sources,cases=cases),indent=2)+'\n')
print('Prepared',len(cases),'original-input combined cases')

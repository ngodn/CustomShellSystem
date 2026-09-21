"""UE 5.6.1: save/reload a constant idle blend space and compare component poses."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
MODE = os.environ.get('CSS_CARRIER_MODE', 'create')
assert MODE in ('create', 'readback')
output = WORK/(MODE+'-result.json')
assert not output.exists()
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
protected_path = WORK/'carrier-protected.json'
if MODE == 'create':
    assert not protected_path.exists()
    protected = json.loads((ROOT/'CustomShellSystem/work/anim4/protected.json').read_text())
    for name in ('CSS/SeduXtress/ABP_Secondary', 'CSS/AnimLab/RT_V2_Idle'):
        path = content/(name+'.uasset')
        protected[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    protected_path.write_text(json.dumps(protected, indent=2)+'\n')
else:
    protected = json.loads(protected_path.read_text())


def verify_protected():
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h
               for p, h in protected.items())


def compare(reference, candidate):
    assert len(reference['frames']) == len(candidate['frames']) == 421
    for key in ('defaults', 'filters', 'fps', 'loops', 'compressed_source'):
        assert reference[key] == candidate[key], key
    maximum = dict(position_cm=0., angle_rad=0., scale=0., morph=0.)
    stages = {name: dict(angle_rad=0., bone='', time=0.) for name in ('upstream', 'secondary')}
    for a, b in zip(reference['frames'], candidate['frames']):
        assert a['time'] == b['time'] and a['clip_time'] == b['clip_time']
        assert a['rigs'] == b['rigs'], 'Secondary rig configuration/counters changed'
        for name in a['morphs'].keys() | b['morphs'].keys():
            maximum['morph'] = max(maximum['morph'], abs(a['morphs'].get(name, 0)-b['morphs'].get(name, 0)))
        for stage, left, right in (('upstream', a['upstream'], b['upstream']),
                                  ('secondary', a['pose']['Snapshot'], b['pose']['Snapshot'])):
            assert left['BoneNames'] == right['BoneNames'] and len(left['BoneNames']) == 379
            for bone, x, y in zip(left['BoneNames'], left['LocalTransforms'], right['LocalTransforms']):
                for field, metric in (('Translation', 'position_cm'), ('Scale3D', 'scale')):
                    delta = math.dist(list(x[field].values()), list(y[field].values()))
                    assert math.isfinite(delta)
                    maximum[metric] = max(maximum[metric], delta)
                q, r = [list(t['Rotation'].values()) for t in (x, y)]
                dot = abs(sum(i*j for i, j in zip(q, r)) / math.sqrt(sum(i*i for i in q)*sum(i*i for i in r)))
                angle = 2*math.acos(min(1., dot))
                maximum['angle_rad'] = max(maximum['angle_rad'], angle)
                if angle > stages[stage]['angle_rad']:
                    stages[stage] = dict(angle_rad=angle, bone=bone, time=a['time'])
    # Sub-millimetre numerical equivalence, not a visual/contact acceptance gate.
    passed = (maximum['position_cm'] < .001 and maximum['angle_rad'] < .001
              and maximum['scale'] < .0001 and maximum['morph'] < .0001)
    return dict(passed=passed, maximum=maximum, stages=stages)


verify_protected()
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
blueprint = unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
animation = unreal.load_asset('/Game/CSS/AnimLab/RT_V2_Idle')
package = '/Game/CSS/AnimLab/BS_V2_Idle'
if MODE == 'create':
    carrier = unreal.CSSAnimationLibrary.create_idle_carrier(mesh, animation, package)
    assert carrier
    assert unreal.EditorAssetLibrary.save_loaded_asset(carrier, only_if_is_dirty=False)
else:
    carrier = unreal.load_asset(package)
    assert carrier
reference = json.loads(unreal.CSSAnimationLibrary.evaluate_clip(mesh, animation, blueprint, 1))
results = []
inputs = [(0, 0, 0)] if MODE == 'create' else [(-180, 0, 0), (180, 800, 0), (47, 233, 0)]
for point in inputs:
    candidate = json.loads(unreal.CSSAnimationLibrary.evaluate_clip(
        mesh, animation, blueprint, 1, carrier, unreal.Vector(*point)))
    assert candidate['carrier'] == package+'.BS_V2_Idle'
    results.append(dict(input=point, **compare(reference, candidate)))
    (WORK/(MODE+'-progress.json')).write_text(json.dumps(results, indent=2)+'\n')
    del candidate
verify_protected()
passed = all(item['passed'] for item in results)
output.write_text(json.dumps(dict(passed=passed, mode=MODE, results=results,
    protected=protected, carrier_sha256=hashlib.sha256(
        (content/'CSS/AnimLab/BS_V2_Idle.uasset').read_bytes()).hexdigest(),
    scope='Offline isolated component equivalence only. No gameplay, montage cancellation, cooking or live acceptance.'), indent=2)+'\n')
assert passed, results

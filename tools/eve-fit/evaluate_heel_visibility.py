"""Check visible, hidden and mid-clip footwear pose states on the Bikini mesh."""
import json
import os
from pathlib import Path
import unreal

work = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
outfit = os.environ.get('CSS_FIT_KIND', 'bikini')
assert outfit in ('bikini', 'knit')
out = work / ('heel-visibility1' if outfit == 'bikini' else 'knit-visibility1')
out.mkdir(exist_ok=False)
mesh = unreal.load_asset('/Game/CSS/EveTest/' + ('SK_BFit1' if outfit == 'bikini' else 'SK_KFit2'))
base = unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
fixed = unreal.load_asset('/Game/CSS/EveTest/ABP_BikiniFeet')
candidate = unreal.load_asset('/Game/CSS/EveTest/' + ('ABP_BikiniFeet2' if outfit == 'bikini' else 'ABP_KnitFeet1'))
assert mesh and base and fixed and candidate
results = {}
for kind in ('Walk', 'Jog', 'Sprint'):
    clip = unreal.load_asset('/Game/CSS/Eve/Anim/AN_'+kind)
    references = []
    for blueprint in (base, fixed):
        raw = unreal.CSSAnimationLibrary.evaluate_clip(mesh, clip, blueprint, 1)
        assert raw
        references.append(json.loads(raw))
    cases = []
    for label, control in [('shown',0), ('hidden',1), ('toggle',2), ('reshown',0)]:
        raw = unreal.CSSAnimationLibrary.evaluate_clip(mesh, clip, candidate, 1, holiday_control=control)
        assert raw, (kind,label)
        record = json.loads(raw)
        worst = 0.0
        mismatches = []
        for index, frame in enumerate(record['frames']):
            shown = frame['shoe_visible']
            if frame['heel_enabled'] != shown: mismatches.append(index)
            actual = frame['pose']['Snapshot']
            expected = references[int(shown)]['frames'][index]['pose']['Snapshot']
            assert actual['BoneNames'] == expected['BoneNames']
            for a,b in zip(actual['LocalTransforms'],expected['LocalTransforms'],strict=True):
                for field in ('Translation','Rotation','Scale3D'):
                    error = max(abs(a[field][axis]-b[field][axis]) for axis in a[field])
                    if field == 'Rotation':
                        error = min(error,max(abs(a[field][axis]+b[field][axis]) for axis in a[field]))
                    worst = max(worst,error)
        report = dict(case=label, frames=len(record['frames']),
            state_mismatch_frames=mismatches, maximum_local_component_error=worst)
        (out / f'{kind.lower()}-{label}.json').write_text(json.dumps(report,indent=2)+'\n')
        assert not mismatches and worst < .0001, (kind,report)
        cases.append(report)
    results[kind] = cases
(out / 'verification.json').write_text(json.dumps(dict(clips=results,
    scope='Native component visibility-to-pose comparison, including newly created instances. Not game rendering, floor contact, CSS persistence or performance acceptance.'),indent=2)+'\n')
print('HEEL_VISIBILITY_PASS', flush=True)

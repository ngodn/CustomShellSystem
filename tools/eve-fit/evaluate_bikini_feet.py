"""Compare the private foot graph with the current secondary graph on real clips."""
import json
import math
from pathlib import Path
import unreal

work = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_HolidayFollow')
base = unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
candidate = unreal.load_asset('/Game/CSS/EveTest/ABP_BikiniFeet')
assert mesh and base and candidate
shared = unreal.load_asset('/Game/CSS/Shared/SKEL_Base')
assert shared and unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh, shared)
report = {}
for kind in ('Walk', 'Jog', 'Sprint'):
    animation = unreal.load_asset('/Game/CSS/Eve/Anim/AN_'+kind)
    assert animation
    records = []
    for label, blueprint in [('base', base), ('candidate', candidate)]:
        path = work / f'feet-{kind.lower()}-{label}.json'
        assert not path.exists()
        raw = unreal.CSSAnimationLibrary.evaluate_clip(mesh, animation, blueprint, 2)
        assert raw, (kind, label)
        record = json.loads(raw)
        assert record['compressed_source'] and record['frames']
        path.write_text(raw)
        records.append(record)
    worst = 0.0
    for a, b in zip(records[0]['frames'], records[1]['frames'], strict=True):
        pa, pb = a['pose']['Snapshot'], b['pose']['Snapshot']
        assert pa['BoneNames'] == pb['BoneNames']
        for name, ta, tb in zip(pa['BoneNames'], pa['LocalTransforms'], pb['LocalTransforms'], strict=True):
            for field in ('Translation', 'Rotation', 'Scale3D'):
                if name in ('foot_l', 'foot_r') and field == 'Rotation':
                    continue
                for axis in ta[field]:
                    delta = abs(ta[field][axis] - tb[field][axis])
                    assert math.isfinite(delta)
                    worst = max(worst, delta)
    assert worst < .0001, (kind, worst)
    report[kind] = dict(frames=len(records[1]['frames']),
        maximum_other_local_component_difference=worst)
(work / 'feet-component.json').write_text(json.dumps(dict(clips=report,
    scope='Compressed clip component evaluation and non-foot-rotation preservation. Foot rotation equivalence, rendered fitting and game behavior require separate checks.'), indent=2)+'\n')
print('HEEL_COMPONENT_PASS', report, flush=True)

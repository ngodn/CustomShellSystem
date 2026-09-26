"""Record original and carrier post-process graphs on the same private mesh."""
import hashlib
import json
import math
from pathlib import Path
import unreal

work=Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
mesh=unreal.load_asset('/Game/CSS/EveTest/SK_HolidayFollow')
shared=unreal.load_asset('/Game/CSS/Shared/SKEL_Base')
baseline=unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
candidate=unreal.load_asset('/Game/CSS/EveTest/ABP_HolidayFollow')
assert mesh and shared and baseline and candidate
assert unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh,shared)
report={}
for kind in ('Walk','Jog','Sprint'):
    animation=unreal.load_asset('/Game/CSS/Eve/Anim/AN_'+kind);assert animation
    records=[]
    for label,bp in [('base',baseline),('candidate',candidate)]:
        path=work/f'follow-{kind.lower()}-{label}.json';assert not path.exists()
        raw=unreal.CSSAnimationLibrary.evaluate_clip(mesh,animation,bp,2);assert raw
        record=json.loads(raw);assert record['frames'] and record['compressed_source']
        path.write_text(raw);records.append(record)
    worst=0.
    for a,b in zip(records[0]['frames'],records[1]['frames'],strict=True):
        pa=a['pose']['Snapshot'];pb=b['pose']['Snapshot']
        assert pa['BoneNames']==pb['BoneNames']
        for name,ta,tb in zip(pa['BoneNames'],pa['LocalTransforms'],pb['LocalTransforms'],strict=True):
            if name.startswith('CSS_Cloth_Skirt_'):continue
            for field in ('Translation','Rotation','Scale3D'):
                for axis in ta[field]:
                    delta=abs(ta[field][axis]-tb[field][axis]);assert math.isfinite(delta)
                    worst=max(worst,delta)
    assert worst<.0001,(kind,worst)
    report[kind]={'frames':len(records[1]['frames']),'non_skirt_max_local_component_difference':worst}
protected=json.loads((work/'motion-protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==v for p,v in protected.items())
(work/'follow-component.json').write_text(json.dumps({'clips':report,
    'scope':'Actual compressed-clip/post-process component evaluation. Non-skirt pose preservation only; skinning equivalence and cloth dynamics need separate checks.'},indent=2)+'\n')
print(report)

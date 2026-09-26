"""Evaluate equal-axis bodies with spring or contact controls across movement clips."""
import hashlib
import json
import math
from pathlib import Path
import unreal

work=Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
mesh=unreal.load_asset('/Game/CSS/EveTest/SK_HolidayBones')
bp=unreal.load_asset('/Game/CSS/EveTest/ABP_Holiday')
shared=unreal.load_asset('/Game/CSS/Shared/SKEL_Base')
clip=unreal.load_asset('/Game/CSS/Eve/Anim/AN_Walk')
assert mesh and bp and shared and clip
assert unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh,shared)
report={}
for control,name,animation_name in [(13,'walk-no-spring','Walk'),(14,'walk-no-contact','Walk'),(13,'jog-no-spring','Jog'),(13,'sprint-no-spring','Sprint')]:
    clip=unreal.load_asset('/Game/CSS/Eve/Anim/AN_'+animation_name)
    assert clip
    path=work/f'motion-cube-{name}.json'
    assert not path.exists(),path
    raw=unreal.CSSAnimationLibrary.evaluate_clip(mesh,clip,bp,1,holiday_control=control)
    assert raw,name
    data=json.loads(raw)
    assert data['holiday_control']==control
    path.write_text(raw)
    largest=0.
    first=0.
    for fi,frame in enumerate(data['frames']):
        pose=frame['pose']['Snapshot']
        for bone,a,b in zip(pose['BoneNames'],pose['LocalTransforms'],frame['upstream']['LocalTransforms'],strict=True):
            if not bone.startswith('CSS_Cloth_Skirt_'):continue
            distance=math.dist([a['Translation'][k] for k in 'XYZ'],[b['Translation'][k] for k in 'XYZ'])
            assert math.isfinite(distance)
            largest=max(largest,distance)
            if fi==1:first=max(first,distance)
    report[name]={'control':control,'frames':len(data['frames']),
                  'first_step_local_shift_cm':first,'max_local_shift_cm':largest,
                  'unaffected_position_cm':data['unaffected_position_cm']}
    print(name,report[name],flush=True)
protected=json.loads((work/'motion-protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==sha for p,sha in protected.items())
(work/'motion-cube.json').write_text(json.dumps(report,indent=2)+'\n')

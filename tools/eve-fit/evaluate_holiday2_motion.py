"""Evaluate the private Holiday secondary graph through the existing compressed-clip probe."""
import hashlib
import json
import math
from pathlib import Path
import unreal

root=Path('/home/eins0fx/development/mods/msII')
work=root/'CustomShellSystem/work/eve26'
mesh=unreal.load_asset('/Game/CSS/EveTest/SK_HolidayBones')
bp=unreal.load_asset('/Game/CSS/EveTest/ABP_Holiday2')
shared=unreal.load_asset('/Game/CSS/Shared/SKEL_Base')
assert mesh and bp and shared
assert unreal.CSSRetargetLibrary.bind_diagnostic_mesh_skeleton(mesh,shared)
info=unreal.CSSRetargetLibrary.inspect_dynamics_defaults(bp)
assert info
(work/'holiday2-dynamics-saved.json').write_text(info)
for name in ('Walk','Jog','Sprint'):
    output=work/f'holiday2-{name.lower()}-motion.json'
    assert not output.exists(),output
    animation=unreal.load_asset('/Game/CSS/Eve/Anim/AN_'+name)
    assert animation
    text=unreal.CSSAnimationLibrary.evaluate_clip(mesh,animation,bp,2)
    assert text, name
    data=json.loads(text)
    assert data['frames'] and data['compressed_source']
    output.write_text(json.dumps(data,separators=(',',':')))
    for frame in data['frames']:
        pose=frame['pose']['Snapshot']
        upstream=frame['upstream']
        for bone, actual, expected in zip(pose['BoneNames'],pose['LocalTransforms'],upstream['LocalTransforms'],strict=True):
            if not bone.startswith('CSS_Cloth_Skirt_'):
                continue
            displacement=math.dist([actual['Translation'][axis] for axis in 'XYZ'],
                                   [expected['Translation'][axis] for axis in 'XYZ'])
            assert displacement < 10, f'{name}: catastrophic skirt displacement {bone}: {displacement} cm'
    unreal.log(f'HOLIDAY_MOTION {name} frames={len(data["frames"])}')
protected=json.loads((work/'motion-protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==sha for p,sha in protected.items())
unreal.log('HOLIDAY_MOTION_DONE_PROTECTED_UNCHANGED')

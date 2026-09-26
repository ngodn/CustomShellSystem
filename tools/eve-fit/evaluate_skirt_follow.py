"""Reload the carrier rig and check its transforms over recorded movement poses."""
import hashlib
import json
import math
from pathlib import Path
import unreal

work=Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
output=work/'skirt-follow-evaluated.json';assert not output.exists()
mesh=json.loads((work/'holiday-hip-clean.mesh.json').read_text())
mapping=json.loads((work/'skirt-follow.json').read_text())['drivers']
names=[b['name'] for b in mesh['bones']]
bp=unreal.load_asset('/Game/CSS/EveTest/CR_HolidayFollow');assert bp
rig=bp.create_control_rig();assert rig
h=rig.get_hierarchy()
key=lambda n:unreal.RigElementKey(type=unreal.RigElementType.BONE,name=n)
xyz=lambda p:(p.x,p.y,p.z)
initial={n:h.get_global_transform(key(n),True) for n in names}
points=[unreal.Vector(*p) for p in [(0,0,0),(100,0,0),(0,100,0),(0,0,100)]]
local={n:[initial[n].inverse_transform_location(p) for p in points] for n in names}
reports={}
for kind in ('walk','jog','sprint'):
    # Only upstream poses are read from the rejected dynamics recordings.
    data=json.loads((work/f'holiday-{kind}-motion.json').read_text())
    worst=preserved=0.
    for frame in data['frames']:
        snapshot=frame['upstream']
        entries=dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
        h.reset_pose_to_initial(unreal.RigElementType.BONE)
        for n in names:
            e=entries[n]
            t=unreal.Transform()
            t.translation=unreal.Vector(*[e['Translation'][k] for k in 'XYZ'])
            t.rotation=unreal.Quat(*[e['Rotation'][k] for k in 'XYZW'])
            t.scale3d=unreal.Vector(*[e['Scale3D'][k] for k in 'XYZ'])
            h.set_local_transform(key(n),t,False,True)
        before={n:h.get_global_transform(key(n)) for n in names}
        assert rig.execute('Forwards Solve')
        for n in names:
            after=h.get_global_transform(key(n))
            if n in mapping:
                driver=mapping[n]
                for a,b in zip(local[n],local[driver],strict=True):
                    worst=max(worst,math.dist(xyz(after.transform_location(a)),xyz(before[driver].transform_location(b))))
            else:
                for p in points:
                    preserved=max(preserved,math.dist(xyz(after.transform_location(p)),xyz(before[n].transform_location(p))))
    reports[kind]={'frames':len(data['frames']),'max_affine_probe_error_cm':worst,'unrelated_bone_error_cm':preserved}
    assert worst<.001 and preserved<.001,reports[kind]
protected=json.loads((work/'motion-protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==v for p,v in protected.items())
output.write_text(json.dumps({'clips':reports,
    'scope':'Freshly reloaded ControlRig, direct execution on recorded upstream movement poses. Four affinely independent points per mapping verify skinning transforms. No component integration, secondary dynamics, collisions or gameplay validation.'},indent=2)+'\n')
print(reports)

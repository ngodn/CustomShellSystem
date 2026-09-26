"""Prepend a diagnostic bind-pose settle and gradual transition to recorded motion."""
import copy
import json
from pathlib import Path

from mathutils import Quaternion, Vector

w = Path(__file__).resolve().parents[2]/'work/eve26'
output = w/'panel-warm-motion.json'
assert not output.exists()
mesh = json.loads((w/'holiday.mesh.json').read_text())
source = json.loads((w/'follow-sprint-base.json').read_text())
first = source['frames'][0]
snapshot = first['pose']['Snapshot']
bones = {b['name']:b for b in mesh['bones']}
frames = []
for frame in range(60):
    row = copy.deepcopy(first)
    row['time'] = frame/60
    blend = max(0., (frame-29)/30)
    result = row['pose']['Snapshot']
    for name, target in zip(result['BoneNames'], result['LocalTransforms'], strict=True):
        bone = bones[name]
        q = bone['rotation']
        start = Quaternion((q[3], *q[:3]))
        end = Quaternion([target['Rotation'][k] for k in 'WXYZ'])
        rotation = start.slerp(end, blend)
        target['Rotation'] = dict(zip('WXYZ', rotation, strict=True))
        for field, values in [('Translation', bone['translation']), ('Scale3D', bone['scale'])]:
            value = Vector(values).lerp(Vector([target[field][k] for k in 'XYZ']), blend)
            target[field] = dict(zip('XYZ', value, strict=True))
    row['upstream'] = copy.deepcopy(result)
    frames.append(row)
for original in source['frames'][:9]:
    row = copy.deepcopy(original)
    row['time'] = 1.+original['time']-first['time']
    frames.append(row)
source['frames'] = frames
source['diagnostic_initialization'] = {
    'scope':'Offline initialization control only, not an authored game animation.',
    'bind_settle_frames':30, 'transition_frames':30, 'recorded_motion_start_frame':60,
    'original':'follow-sprint-base.json',
}
output.write_text(json.dumps(source)+'\n')
print('Wrote', len(frames), 'frames; original sprint frame 4 is diagnostic frame 64')

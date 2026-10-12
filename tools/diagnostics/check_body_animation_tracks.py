"""Measure local body-helper motion in retained animation sources, without physics."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal


stage = Path(os.environ['CSS_TRACK_STAGE'])
content = Path(os.environ['CSS_TRACK_CONTENT'])
packages = [
    '/Game/CSS/eins0fx/Shared/Locomotion/Eve/idle/AN_Default',
    '/Game/CSS/eins0fx/Shared/Locomotion/Studies/L5X/AN_IrisOut',
    '/Game/CSS/eins0fx/Shared/Locomotion/Studies/Contact3/AN_BlackHole',
]
bones = ['brust001', 'brust002', 'butt001', 'butt002',
         'thigh_twist_02_l', 'thigh_twist_02_r', 'belly', 'pelvis']


def values(transform):
    p, q, s = transform.translation, transform.rotation, transform.scale3d
    return [p.x, p.y, p.z, q.x, q.y, q.z, q.w, s.x, s.y, s.z]


def differences(first, current):
    dot = abs(sum(a*b for a, b in zip(first[3:7], current[3:7])))
    return [math.dist(first[:3], current[:3]),
            math.degrees(2*math.acos(min(1., dot))),
            max(abs(a-b) for a, b in zip(first[7:], current[7:]))]


result = dict(scope='Raw local animation poses before any body Control Rig', clips=[])
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('should_retarget', False)
for package in packages:
    path = content / (package.removeprefix('/Game/')+'.uasset')
    before = hashlib.sha256(path.read_bytes()).hexdigest()
    clip = unreal.load_asset(package)
    if not isinstance(clip, unreal.AnimSequence):
        raise TypeError(package)
    snapshot = json.loads(unreal.CSSRigAuditLibrary.snapshot_animation(clip))
    tracks = {row['bone'] for row in snapshot['tracks']}
    missing = set(bones)-tracks
    if missing:
        raise ValueError(f'{package}: missing body tracks {missing}')
    rate = snapshot['rate_numerator']/snapshot['rate_denominator']
    first, peaks = {}, {bone: [0., 0., 0.] for bone in bones}
    for frame in range(snapshot['keys']):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, frame/rate, options)
        if not unreal.AnimPoseExtensions.is_valid(pose):
            raise ValueError(f'Invalid pose at frame {frame}: {package}')
        if frame == 0 and not set(bones).issubset(
                {str(name) for name in unreal.AnimPoseExtensions.get_bone_names(pose)}):
            raise ValueError('Evaluated pose omits requested bones')
        for bone in bones:
            current = values(unreal.AnimPoseExtensions.get_bone_pose(
                pose, bone, unreal.AnimPoseSpaces.LOCAL))
            if not all(math.isfinite(value) for value in current):
                raise ValueError('Non-finite pose')
            first.setdefault(bone, current)
            peaks[bone] = [max(a, b) for a, b in
                           zip(peaks[bone], differences(first[bone], current))]
    if hashlib.sha256(path.read_bytes()).hexdigest() != before:
        raise RuntimeError('Source animation changed')
    if max(peaks['pelvis'][:2]) < .001:
        raise ValueError('Positive control pelvis did not move')
    result['clips'].append(dict(package=package, sha256=before, keys=snapshot['keys'],
                                rate=rate, first=first,
                                peak_from_first_cm_degrees_scale=peaks))
    (stage/'tracks.json').write_text(json.dumps(result, indent=2)+'\n')
    unreal.log('Body animation tracks checked: '+package)

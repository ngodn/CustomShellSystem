"""Isolate ACL float-time sampling from authored-key error on Cinderella."""
import json
import math
import os
from pathlib import Path
import struct

import unreal

work = Path(os.environ['CSS_NATIVE_LIBRARY_WORK']).resolve(strict=True)
plan = json.loads(Path(os.environ['CSS_NATIVE_LIBRARY_PLAN']).read_text())
target = unreal.load_asset(plan['target_mesh'])
source = unreal.load_asset('/Game/CSS/eins0fx/Shared/Locomotion/NativeHuman/Studies/C11/AN_Cinderella')
tools = unreal.AssetToolsHelpers.get_asset_tools()
clip = tools.duplicate_asset('TimeCheck', '/Game/CSSAuthoring/TransientProbes', source)
if not unreal.CSSRetargetLibrary.prepare_compressed_animation(clip, target):
    raise RuntimeError('Compressed evaluation unavailable')
raw, packed = unreal.AnimPoseEvaluationOptions(), unreal.AnimPoseEvaluationOptions()
for options, kind in ((raw, unreal.AnimDataEvalType.RAW), (packed, unreal.AnimDataEvalType.COMPRESSED)):
    options.set_editor_property('evaluation_type', kind)
    options.set_editor_property('optional_skeletal_mesh', target)
    options.set_editor_property('should_retarget', True)


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def values(transform):
    t, q = transform.translation, transform.rotation
    return (t.x, t.y, t.z), (q.x, q.y, q.z, q.w)


results = []
for frame, bone in ((7746, 'VB hand_r_relative_hand_l'), (2493, 'pinky_03_r'),
                    (7746, 'hand_r'), (7746, 'hand_l')):
    nominal = frame / 60
    times = dict(nominal=nominal, float_seconds=f32(nominal),
                 acl_sample_index=f32(f32(nominal) * 60) / 60)
    compressed = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, nominal, packed)
    y, qb = values(unreal.AnimPoseExtensions.get_bone_pose(compressed, bone, unreal.AnimPoseSpaces.LOCAL))
    comparisons = {}
    for mode, time in times.items():
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, time, raw)
        x, qa = values(unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL))
        comparisons[mode] = dict(time=time, time_offset=time-nominal, raw_position=x,
            position_error=math.dist(x, y), quaternion_error=min(math.dist(qa, qb), math.dist(qa, [-v for v in qb])))
    results.append(dict(frame=frame, bone=bone, compressed_position=y, comparisons=comparisons))
(work/'result.json').write_text(json.dumps(dict(samples=results,
    scope='Targeted clock-alignment diagnostic, not an acceptance gate.'), indent=2)+'\n')

"""UE 5.6.1: independently compare the original and derived source world poses."""
import hashlib
import json
import math
import os
import re
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.resolve().is_relative_to((ROOT / 'CustomShellSystem/work').resolve())
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
revision = json.loads((WORK / 'batch-config.json').read_text())['revision']
original_revision = os.environ['CSS_SOURCE_REVISION']
assert all(re.fullmatch(r'[A-Za-z0-9]{1,8}', name) for name in (revision, original_revision))
assert revision != original_revision
protected = {}


def load(package):
    file = CONTENT / (package.removeprefix('/Game/') + '.uasset')
    protected[str(file)] = hashlib.sha256(file.read_bytes()).hexdigest()
    asset = unreal.load_asset(package)
    assert asset, package
    return asset


meshes = [load(f'/Game/CSS/AnimLab/SK_{r}_EveSource') for r in (original_revision, revision)]
options = []
for mesh in meshes:
    item = unreal.AnimPoseEvaluationOptions()
    item.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    item.set_editor_property('optional_skeletal_mesh', mesh)
    item.set_editor_property('extract_root_motion', False)
    options.append(item)
results = []
for label, filename in json.loads((WORK / 'batch.json').read_text()):
    assert re.fullmatch(r'[A-Za-z][A-Za-z0-9]{0,15}', label)
    clips = [load(f'/Game/CSS/AnimLab/AN_{r}_{label}') for r in (original_revision, revision)]
    clock = json.loads((WORK / (filename + '.json')).read_text())
    maxima = {'keys': [0., 0., 0.], 'half_frames': [0., 0., 0.]}
    worst_position = {}
    for half_frame in range(2 * clock['frames'] - 1):
        seconds = half_frame / (2 * clock['fps'])
        poses = [(unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, seconds, option)
                  if half_frame % 2 else unreal.AnimPoseExtensions.get_anim_pose_at_frame(clip, half_frame // 2, option))
                 for clip, option in zip(clips, options)]
        assert all(unreal.AnimPoseExtensions.is_valid(pose) for pose in poses)
        names = [[str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)] for pose in poses]
        assert names[0] == names[1]
        kind = 'half_frames' if half_frame % 2 else 'keys'
        for name in names[0]:
            a, b = [unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
                    for pose in poses]
            q = [a.rotation.x, a.rotation.y, a.rotation.z, a.rotation.w]
            r = [b.rotation.x, b.rotation.y, b.rotation.z, b.rotation.w]
            errors = [math.dist([a.translation.x, a.translation.y, a.translation.z],
                                [b.translation.x, b.translation.y, b.translation.z]),
                      min(math.dist(q, r), math.dist(q, [-v for v in r])),
                      math.dist([a.scale3d.x, a.scale3d.y, a.scale3d.z],
                                [b.scale3d.x, b.scale3d.y, b.scale3d.z])]
            if errors[0] > maxima[kind][0]:
                worst_position[kind] = dict(bone=name, frame=half_frame / 2, errors=errors)
            maxima[kind] = [max(x, y) for x, y in zip(maxima[kind], errors)]
    results.append(dict(clip=label, keys=clock['frames'], bones=len(names[0]),
                        max_errors_position_cm_quaternion_scale=maxima, worst_position=worst_position,
                        keys_pass=all(value < limit for value, limit in zip(maxima['keys'], (.001, .0001, .0001)))))
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
with (WORK / 'source-world-verification.json').open('x') as stream:
    json.dump(dict(results=results, protected=protected,
                   scope='Fresh UE raw world poses at every source key and half-frame. Half-frame differences are measured, not assumed to be zero.'), stream, indent=2)
assert all(result['keys_pass'] for result in results), results

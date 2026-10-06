"""Capture unretargeted raw UE poses for comparison with independently decoded cooks."""
import gzip
import hashlib
import json
import os
from pathlib import Path

import unreal

work = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
receipt = json.loads((work / 'verify-result.json').read_text())
if not receipt['complete']:
    raise ValueError('Authoring verification is incomplete')
output = work / 'raw-cook-reference'
output.mkdir(exist_ok=False)
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('should_retarget', False)
options.set_editor_property('extract_root_motion', False)
options.set_editor_property('incorporate_root_motion_into_pose', True)
results = []
for index, entry in enumerate(receipt['results']):
    if not entry['asset'].startswith('/Game/CSS/Locomotion/Studies/') or not 2 <= entry['keys'] <= 10000:
        raise ValueError('Expected an isolated study with bounded key count')
    clip = unreal.load_asset(entry['asset'])
    if not clip or abs(unreal.AnimationLibrary.get_sequence_length(clip) - entry['duration']) > .00001:
        raise ValueError('Missing or changed study animation')
    path = output / f'{index:02d}.json.gz'
    with gzip.open(path, 'xt', compresslevel=3) as stream:
        stream.write(json.dumps(dict(fps=entry['hz'], should_retarget=False,
            sampling='Raw UE poses at source key times, no optional mesh and no retargeting'))[:-1] + ',"frames":[')
        order = None
        for frame in range(entry['keys']):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(clip, frame, options)
            if not unreal.AnimPoseExtensions.is_valid(pose):
                raise ValueError('Invalid raw pose')
            names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
            if order is not None and names != order:
                raise ValueError('Raw bone order changed')
            order = names
            transforms = []
            for name in names:
                t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
                transforms.append(dict(Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                    Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                    Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z)))))
            if frame:
                stream.write(',')
            json.dump(dict(pose=dict(Snapshot=dict(bIsValid=True, BoneNames=names, LocalTransforms=transforms))),
                      stream, separators=(',', ':'))
        stream.write(']}')
    results.append(dict(id=entry['id'], frames=entry['keys'], tracks=len(order), file=path.name))
for path, expected in receipt['protected'].items():
    with Path(path).open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != expected:
            raise ValueError('Protected input changed: ' + path)
(output / 'result.json').write_text(json.dumps(dict(complete=True, results=results,
    scope='Unretargeted raw study poses for cook comparison. Not mesh contact or gameplay.'), indent=2) + '\n')

"""Compare independently decoded cooked samples with UE raw poses, without saving assets."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
SAMPLES = Path(os.environ['CSS_COOKED_SAMPLES']).resolve()
report_path = SAMPLES / 'motion-verification.json'
if report_path.exists():
    raise FileExistsError(report_path)
authored = json.loads((WORK / 'verify-result.json').read_text())
properties = json.loads((WORK / 'cooked-properties.json').read_text())
decoded = json.loads((SAMPLES / 'decoded.json').read_text())
if not authored['complete'] or not decoded['complete']:
    raise ValueError('Incomplete authored or cooked readback')
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('should_retarget', False)
options.set_editor_property('extract_root_motion', False)
results = []
for index, entry in enumerate(authored['results']):
    path = entry['asset'].split('.')[0]
    clip = unreal.load_asset(path)
    sample = json.loads((SAMPLES / f'{index:02d}' / 'absolute-tracks.json').read_text())
    prop = next(e for e in properties[path] if e['Type'] == 'AnimSequence')
    if (sample['logicalSamples'] != entry['keys'] or abs(sample['sampleRate'] - entry['hz']) > .001
            or abs(prop['Properties']['SequenceLength'] - entry['duration']) > .00001
            or sample['virtualTrackCount'] != 9):
        raise ValueError('Cooked timing or virtual tracks changed: ' + path)
    maxima = dict(position_cm=0.0, quaternion_distance=0.0, scale=0.0)
    worst = {}
    for frame in sample['frames']:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, frame['timeSeconds'], options)
        if not unreal.AnimPoseExtensions.is_valid(pose):
            raise ValueError('Invalid raw pose: ' + path)
        for name, expected in zip(frame['BoneNames'], frame['LocalTransforms'], strict=True):
            actual = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
            a = [actual.rotation.x, actual.rotation.y, actual.rotation.z, actual.rotation.w]
            b = [expected['Rotation'][k] for k in 'XYZW']
            errors = dict(position_cm=math.dist([actual.translation.x, actual.translation.y, actual.translation.z],
                                               [expected['Translation'][k] for k in 'XYZ']),
                          quaternion_distance=min(math.dist(a, b), math.dist(a, [-v for v in b])),
                          scale=math.dist([actual.scale3d.x, actual.scale3d.y, actual.scale3d.z],
                                          [expected['Scale3D'][k] for k in 'XYZ']))
            for key, value in errors.items():
                if not math.isfinite(value):
                    raise ValueError('Nonfinite decoded transform')
                if value > maxima[key]:
                    maxima[key] = value
                    worst[key] = dict(bone=name, frame=frame['frame'])
    # Review limits: 0.5 mm translation, 0.25 degree local rotation, 0.1% scale.
    # These flag candidates for inspection; they do not establish visual acceptance.
    passed = maxima['position_cm'] < .05 and maxima['quaternion_distance'] < 2 * math.sin(math.radians(.25) / 4) and maxima['scale'] < .001
    results.append(dict(id=entry['id'], samples=len(sample['frames']), tracks=sample['trackCount'],
                        maxima=maxima, worst=worst, passed=passed))
    report_path.write_text(json.dumps(dict(complete=False, results=results), indent=2) + '\n')
    print('COOKED_MOTION', entry['id'], maxima, passed, flush=True)
for path, checksum in authored['protected'].items():
    with Path(path).open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != checksum:
            raise ValueError('Protected authoring input changed: ' + path)
report_path.write_text(json.dumps(dict(complete=True, passed=all(r['passed'] for r in results),
                                     results=results, scope='Up to five cooked samples per clip against raw UE poses, including nine virtual tracks. No gameplay or visual acceptance.'), indent=2) + '\n')
if not all(r['passed'] for r in results):
    raise ValueError('Cooked pose error exceeds review tolerance; see report')

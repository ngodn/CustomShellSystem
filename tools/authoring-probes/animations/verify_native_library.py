"""Reload native candidates and compare authored and compressed local poses."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

work = Path(os.environ['CSS_NATIVE_LIBRARY_WORK']).resolve(strict=True)
plan = json.loads(Path(os.environ['CSS_NATIVE_LIBRARY_PLAN']).read_text())
reports = [Path(p) for p in json.loads(os.environ['CSS_NATIVE_LIBRARY_REPORTS'])]
rows = [row for report in reports for row in json.loads(report.read_text())['clips']]
if not rows or len({row['id'] for row in rows}) != len(rows):
    raise ValueError('Empty or duplicate candidate selection')
content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve()
if content != Path(plan['content']).resolve():
    raise ValueError('Wrong authoring project')
target = unreal.load_asset(plan['target_mesh'])
native = unreal.load_asset(plan['native_skeleton'])
if not target or not native:
    raise ValueError('Missing native references')
tools = unreal.AssetToolsHelpers.get_asset_tools()
raw, compressed = unreal.AnimPoseEvaluationOptions(), unreal.AnimPoseEvaluationOptions()
for options, kind in ((raw, unreal.AnimDataEvalType.RAW),
                      (compressed, unreal.AnimDataEvalType.COMPRESSED)):
    options.set_editor_property('evaluation_type', kind)
    options.set_editor_property('optional_skeletal_mesh', target)
    options.set_editor_property('should_retarget', True)
    options.set_editor_property('extract_root_motion', False)
results = []


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def vec(v):
    return (v.x, v.y, v.z)


def write(name, value):
    (work / name).write_text(json.dumps(value, indent=2) + '\n')


protected = dict(plan['protected'])
for row in rows:
    package = content / (row['package'].removeprefix('/Game/') + '.uasset')
    protected[str(package)] = row['sha256']
    if sha(package) != row['sha256'] or sha(row['snapshot']) != row['snapshot_sha256']:
        raise ValueError('Candidate or authored receipt changed')
    saved = unreal.load_asset(row['package'])
    if not saved or saved.get_editor_property('skeleton') != native:
        raise ValueError('Candidate has wrong skeleton')
    expected = json.loads(Path(row['snapshot']).read_text())
    actual = json.loads(unreal.CSSRigAuditLibrary.snapshot_animation(saved))
    if actual != expected:
        raise ValueError('Saved authored keys differ: ' + row['id'])
    clip = tools.duplicate_asset('Verify_' + hashlib.sha256(row['id'].encode()).hexdigest()[:12],
                                 '/Game/CSSAuthoring/TransientProbes', saved)
    if not clip or not unreal.CSSRetargetLibrary.prepare_compressed_animation(clip, target):
        raise RuntimeError('Compression unavailable: ' + row['id'])
    if os.environ.get('CSS_NATIVE_LIBRARY_CPP') == '1':
        data = unreal.CSSRigAuditLibrary.compare_compressed_poses(saved, clip, target)
        if data.startswith('ERROR:'):
            raise RuntimeError(data)
        result = json.loads(data)
        if result['frames'] != row['keys'] or result['bones'] != 1208:
            raise ValueError('Incomplete compressed comparison')
        results.append(dict(id=row['id'], saved_keys_equal=True, **result))
        write('results.json', results)
        write('progress.json', dict(completed=len(results), total=len(rows), id=row['id']))
        if not result['compressed_passed']:
            raise ValueError('Compression exceeds established bounds: ' + row['id'])
        continue
    errors = dict(position_cm=0.0, quaternion_distance=0.0, scale=0.0)
    worst = {}
    names = None
    for frame in range(row['keys']):
        time = frame * row['fps'][1] / row['fps'][0]
        a = unreal.AnimPoseExtensions.get_anim_pose_at_time(saved, time, raw)
        b = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, time, compressed)
        if not unreal.AnimPoseExtensions.is_valid(a) or not unreal.AnimPoseExtensions.is_valid(b):
            raise ValueError('Invalid evaluated pose')
        current = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(a)]
        if names is None:
            names = current
        if current != names or len(names) != 1208 or len(set(names)) != 1208:
            raise ValueError('Unexpected native bone mapping')
        if current != [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(b)]:
            raise ValueError('Compressed bone mapping differs')
        for name in names:
            x = unreal.AnimPoseExtensions.get_bone_pose(a, name, unreal.AnimPoseSpaces.LOCAL)
            y = unreal.AnimPoseExtensions.get_bone_pose(b, name, unreal.AnimPoseSpaces.LOCAL)
            qa, qb = (*vec(x.rotation), x.rotation.w), (*vec(y.rotation), y.rotation.w)
            values = (math.dist(vec(x.translation), vec(y.translation)),
                      min(math.dist(qa, qb), math.dist(qa, [-v for v in qb])),
                      math.dist(vec(x.scale3d), vec(y.scale3d)))
            if not all(math.isfinite(v) for v in values):
                raise ValueError('Nonfinite evaluated transform')
            for key, value in zip(errors, values):
                if value > errors[key]:
                    errors[key], worst[key] = value, dict(frame=frame, bone=name)
        if frame % 100 == 0:
            write('progress.json', dict(completed=len(results), total=len(rows),
                                        id=row['id'], frame=frame, frames=row['keys']))
    passed = errors['position_cm'] < .01 and errors['quaternion_distance'] < .001 and errors['scale'] < .00001
    results.append(dict(id=row['id'], frames=row['keys'], bones=len(names),
                        saved_keys_equal=True, compressed_passed=passed, errors=errors, worst=worst))
    write('results.json', results)
    if not passed:
        raise ValueError('Compression exceeds established bounds: ' + row['id'])
for path, expected in protected.items():
    if sha(path) != expected:
        raise ValueError('Protected input changed: ' + path)
write('result.json', dict(passed=True, clips=results, protected_unchanged=True,
    scope='Fresh authored-key readback and editor compressed local poses at every source key. Windows cook, visible contact and gameplay remain.'))

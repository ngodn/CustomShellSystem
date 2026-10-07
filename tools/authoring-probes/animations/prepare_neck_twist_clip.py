"""UE 5.6.1: save and verify an isolated three-rotation neck correction."""
import gzip
import hashlib
import json
import math
import os
from pathlib import Path

import unreal


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


work = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
phase = os.environ.get('CSS_NECK_PHASE', 'prepare')
if phase not in ('prepare', 'verify') or (work / (phase + '-result.json')).exists():
    raise ValueError('Use a fresh prepare or verify phase')
root = Path(__file__).resolve().parents[3]
content = root.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
plan = json.loads((work / 'plan.json').read_text())
row = plan['source']
package = plan['destination']
if (row['id'] != 'css.eve.dance.jangwonyoungspin' or row['keys'] != 111
        or package != '/Game/CSS/Locomotion/Studies/Neck1/AN_JangWonYoungSpin'):
    raise ValueError('Expected the reviewed Spin correction')
protected = dict(plan['inputs'])
protected[str(work / 'plan.json')] = sha(work / 'plan.json')
source_path = content / (row['asset'].split('.')[0].removeprefix('/Game/') + '.uasset')
protected[str(source_path)] = row['sha256']
settings_path = '/Game/CSS/Locomotion/Studies/Precision1/ACS_Motion'
settings_disk = content / (settings_path.removeprefix('/Game/') + '.uasset')
protected[str(settings_disk)] = sha(settings_disk)
for path, checksum in protected.items():
    if sha(Path(path)) != checksum:
        raise ValueError('Protected input changed: ' + path)
with gzip.open(plan['reference'], 'rt') as stream:
    reference = json.load(stream)
motion = json.loads(Path(plan['motion']).read_text())
if len(reference['frames']) != 111 or len(motion['frames']) != 111 or motion['fps'] != 30:
    raise ValueError('Incomplete Spin motion')
changed = ('neck_01', 'neck_02', 'head')
source = unreal.load_asset(row['asset'])
settings = unreal.load_asset(settings_path)
if not source or not settings or abs(unreal.AnimationLibrary.get_sequence_length(source) - row['duration']) > 1e-8:
    raise ValueError('Missing or changed source/settings')
destination = content / (package.removeprefix('/Game/') + '.uasset')
if phase == 'prepare':
    if destination.exists():
        raise FileExistsError(destination)
    candidate = unreal.EditorAssetLibrary.duplicate_asset(row['asset'], package)
    if not candidate or not unreal.CSSAnimationLibrary.compare_sequence_data(source, candidate).startswith('PASS:'):
        raise ValueError('Initial copy differs')
    controller = candidate.get_editor_property('controller')
    controller.open_bracket('Correct Spin neck twist while preserving its head pose', False)
    try:
        for name in changed:
            original, target = [], []
            for raw, fixed in zip(reference['frames'], motion['frames'], strict=True):
                a, b = raw['pose']['Snapshot'], fixed['pose']['Snapshot']
                original.append(a['LocalTransforms'][a['BoneNames'].index(name)])
                target.append(b['LocalTransforms'][b['BoneNames'].index(name)])
            if not controller.set_bone_track_keys(name,
                    [unreal.Vector(*[t['Translation'][k] for k in 'XYZ']) for t in original],
                    [unreal.Quat(*[t['Rotation'][k] for k in 'XYZW']) for t in target],
                    [unreal.Vector(*[t['Scale3D'][k] for k in 'XYZ']) for t in original], False):
                raise ValueError('Could not write neck rotation: ' + name)
    finally:
        controller.close_bracket(False)
    candidate.set_editor_property('bone_compression_settings', settings)
    if not unreal.EditorAssetLibrary.save_loaded_asset(candidate, False):
        raise ValueError('Could not save candidate')
else:
    prepared = json.loads((work / 'prepare-result.json').read_text())
    if sha(destination) != prepared['results'][0]['sha256']:
        raise ValueError('Saved candidate changed')
    candidate = unreal.load_asset(package)
if not candidate or candidate.get_editor_property('bone_compression_settings') != settings:
    raise ValueError('Unexpected saved compression settings')
if abs(unreal.AnimationLibrary.get_sequence_length(candidate) - row['duration']) > 1e-8:
    raise ValueError('Duration changed')
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('should_retarget', False)
options.set_editor_property('extract_root_motion', False)
options.set_editor_property('incorporate_root_motion_into_pose', True)
frames = []
maxima = dict(unchanged_position_cm=0., unchanged_rotation_distance=0.,
              unchanged_scale=0., corrected_rotation_distance=0., source_reference_distance=0.)
for frame in range(111):
    poses = [unreal.AnimPoseExtensions.get_anim_pose_at_frame(clip, frame, options) for clip in (source, candidate)]
    if not all(unreal.AnimPoseExtensions.is_valid(pose) for pose in poses):
        raise ValueError('Invalid pose')
    names = [[str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)] for pose in poses]
    expected = reference['frames'][frame]['pose']['Snapshot']
    fixed = motion['frames'][frame]['pose']['Snapshot']
    if names[0] != names[1] or names[0] != expected['BoneNames']:
        raise ValueError('Bone order changed')
    transforms = []
    for index, name in enumerate(names[0]):
        values = []
        for pose in poses:
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
            values.append(dict(Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z)))))
        a, b = values
        for field, key in [('Translation', 'unchanged_position_cm'), ('Scale3D', 'unchanged_scale'),
                           ('Rotation', 'unchanged_rotation_distance')]:
            original = list(a[field].values())
            saved = list(expected['LocalTransforms'][index][field].values())
            maxima['source_reference_distance'] = max(maxima['source_reference_distance'], math.dist(original, saved))
            wanted = original
            if field == 'Rotation' and name in changed:
                wanted = list(fixed['LocalTransforms'][fixed['BoneNames'].index(name)][field].values())
                key = 'corrected_rotation_distance'
            result = list(b[field].values())
            distance = math.dist(wanted, result)
            if field == 'Rotation':
                distance = min(distance, math.dist(wanted, [-v for v in result]))
            maxima[key] = max(maxima[key], distance)
        transforms.append(b)
    frames.append(dict(pose=dict(Snapshot=dict(bIsValid=True, SkeletalMeshName='SK_BlackPearl2',
        BoneNames=names[0], LocalTransforms=transforms))))
if max(maxima.values()) > .00001:
    raise ValueError('Saved pose differs: ' + json.dumps(maxima))
with gzip.open(work / (phase + '-motion.json.gz'), 'xt', compresslevel=3) as stream:
    json.dump(dict(fps=30, should_retarget=False, frames=frames), stream, separators=(',', ':'))
for path, checksum in protected.items():
    if sha(Path(path)) != checksum:
        raise ValueError('Protected input changed: ' + path)
protected[str(destination)] = sha(destination)
result = {**row, 'asset': candidate.get_path_name(), 'sha256': sha(destination),
          'result': 'PASS: full raw pose comparison, three allowed rotation tracks only', 'maxima': maxima}
(work / (phase + '-result.json')).write_text(json.dumps(dict(complete=True, results=[result],
    protected=protected, installed=False, scope='Isolated neck correction, full raw key verification. Cook and gameplay pending.'), indent=2) + '\n')
if phase == 'prepare':
    catalog = json.loads((root / 'work/cine-animation-port/shared-v1/catalog.json').read_text())
    entry = next(e for e in catalog['animation_library']['entries'] if e['id'] == row['id'])
    entry['assets'][0]['asset'] = candidate.get_path_name()
    catalog['animation_library']['entries'] = [entry]
    (work / 'catalog.json').write_text(json.dumps(catalog, indent=2) + '\n')
    (work / 'cook.txt').write_text(package + '\n')
print('NECK_CORRECTION', phase, json.dumps(maxima), flush=True)

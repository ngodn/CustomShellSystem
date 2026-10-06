"""Reload native carriers and capture every sequence key for cook comparison."""
import gzip
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
REPO = ROOT / 'CustomShellSystem'
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
WORK = Path(os.environ['CSS_NATIVE_LOCOMOTION_VERIFY_WORK']).resolve()
BLENDS = Path(os.environ['CSS_NATIVE_LOCOMOTION_BLEND_WORK']).resolve()
for path in (WORK, BLENDS):
    assert path.is_relative_to(REPO / 'work/cine-animation-port')
assert not (WORK / 'result.json').exists()
assert json.loads((BLENDS / 'exit.json').read_text())['exit_code'] == 0
blends = json.loads((BLENDS / 'result.json').read_text())
sequences = json.loads(Path(blends['sequence_report']).read_text())
assert blends['passed'] and sequences['authored']
protected = dict(blends['protected'])


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(name, value):
    (WORK / name).write_text(json.dumps(value, indent=2) + '\n')


for row in blends['blends']:
    path = CONTENT / (row['package'].removeprefix('/Game/') + '.uasset')
    assert sha(path) == row['sha256']
    protected[str(path)] = row['sha256']
    asset = unreal.load_asset(row['package'])
    assert asset
    reference = json.loads((BLENDS / (row['slot'].lower() + '-queries.json')).read_text())
    inputs = [unreal.Vector(q['direction'], q['speed'], 0) for q in reference['candidate']['queries']]
    actual = json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(asset, inputs))
    assert actual == reference['candidate'], row['slot']

options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('should_retarget', False)
options.set_editor_property('extract_root_motion', False)
results = []
for row in sequences['clips']:
    clip = unreal.load_asset(row['package'])
    original = unreal.load_asset(row['source'])
    assert clip and original
    frames = unreal.AnimationLibrary.get_num_keys(clip)
    assert frames == unreal.AnimationLibrary.get_num_keys(original) and 2 <= frames <= 2000
    assert math.isclose(clip.get_play_length(), original.get_play_length(), abs_tol=1e-6)
    fps = round((frames - 1) / clip.get_play_length(), 6)
    names = None
    output = WORK / (row['label'] + '-raw.json.gz')
    assert not output.exists()
    with gzip.open(output, 'wt') as stream:
        stream.write(json.dumps(dict(fps=fps, should_retarget=False))[:-1] + ', "frames": [')
        for frame in range(frames):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, frame / fps, options)
            assert unreal.AnimPoseExtensions.is_valid(pose)
            current = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
            assert len(current) == 1208 and len(set(current)) == 1208
            if names is None:
                names = current
            assert current == names
            transforms = []
            for name in names:
                t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
                values = (t.translation.x, t.translation.y, t.translation.z,
                          t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w,
                          t.scale3d.x, t.scale3d.y, t.scale3d.z)
                assert all(math.isfinite(v) for v in values)
                assert abs(sum(v*v for v in values[3:7]) - 1) < .001
                transforms.append(dict(Translation=dict(zip('XYZ', values[:3])),
                                       Rotation=dict(zip('XYZW', values[3:7])),
                                       Scale3D=dict(zip('XYZ', values[7:]))))
            if frame:
                stream.write(',')
            json.dump(dict(pose=dict(Snapshot=dict(bIsValid=True, BoneNames=names,
                                                   LocalTransforms=transforms))), stream)
            if frame % 50 == 0:
                write('progress.json', dict(completed=len(results), total=20,
                                            clip=row['label'], frame=frame, frames=frames))
        stream.write(']}')
    results.append(dict(label=row['label'], package=row['package'], frames=frames, fps=fps,
                        bones=len(names), raw=str(output), sha256=sha(output)))
    print('NATIVE_CAPTURE', row['label'], frames, flush=True)
assert all(sha(Path(path)) == digest for path, digest in protected.items())
write('result.json', dict(passed=True, clips=results, blend_queries=624, protected=protected,
      scope='Fresh raw sequence and blend readback. Cooked motion, visible meshes, contact and runtime acceptance remain pending.'))

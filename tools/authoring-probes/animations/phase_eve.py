"""UE 5.6.1: create and independently read back whole-key phase corrections."""
import hashlib
import json
import math
import os
from pathlib import Path
import runpy

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
MODE = os.environ.get('CSS_MOVEMENT_MODE', 'create')
assert MODE in ('create', 'readback')
assert not (WORK/(MODE+'-phase-result.json')).exists()
phase = json.loads((WORK/'phase.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in phase['inputs'].items())
protected = json.loads((ROOT/'CustomShellSystem/work/anim11/protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh', mesh)


def check(animation, source, shift):
    assert animation.get_editor_property('skeleton') == mesh.get_editor_property('skeleton')
    frames = source['frames']
    assert math.isclose(animation.get_play_length(), (len(frames)-1)/source['fps'], abs_tol=1e-6)
    maximum_position = maximum_rotation = 0.
    for frame in range(len(frames)):
        index = (frame+shift) % (len(frames)-1) if shift else frame
        expected = frames[index]['pose']['Snapshot']
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(animation, frame/source['fps'], options)
        assert unreal.AnimPoseExtensions.is_valid(pose)
        names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
        assert names == expected['BoneNames']
        for name, value in zip(names, expected['LocalTransforms'], strict=True):
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
            distance = math.dist([t.translation.x, t.translation.y, t.translation.z],
                                 [value['Translation'][k] for k in 'XYZ'])
            a = [t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w]
            b = [value['Rotation'][k] for k in 'XYZW']
            rotation = min(math.dist(a, b), math.dist(a, [-v for v in b]))
            assert distance < .001 and rotation < .0001, (frame, name, distance, rotation)
            assert math.dist([t.scale3d.x, t.scale3d.y, t.scale3d.z],
                             [value['Scale3D'][k] for k in 'XYZ']) < .0001
            maximum_position = max(maximum_position, distance)
            maximum_rotation = max(maximum_rotation, rotation)
    return dict(frames=len(frames), bones=len(names), max_position_cm=maximum_position,
                max_quaternion_distance=maximum_rotation)


results = []
if MODE == 'create':
    source = unreal.load_asset('/Game/CSS/AnimLab/RT_D2_Walk')
    for name, offset, output in [('Negative', -1, 'RT_P1_BadNegative'),
                                ('Range', 36, 'RT_P1_BadRange'),
                                ('Path', 1, 'InvalidPhaseOutput')]:
        assert not unreal.CSSAnimationLibrary.shift_loop(source, offset, '/Game/CSS/AnimLab/'+output), name
    zero = unreal.CSSAnimationLibrary.shift_loop(source, 0, '/Game/CSS/AnimLab/RT_P1_Zero')
    assert zero
    document = json.loads((ROOT/'CustomShellSystem/work/anim10/batch/walk-motion.json').read_text())
    results.append(dict(clip='Zero', **check(zero, document, 0)))
for label, entry in phase['clips'].items():
    shift = entry['start_frame']
    if not shift:
        continue
    path = '/Game/CSS/AnimLab/RT_P1_'+label
    source = unreal.load_asset('/Game/CSS/AnimLab/RT_D2_'+label)
    assert source
    document = json.loads((ROOT/'CustomShellSystem/work/anim10/batch'/(label.lower()+'-motion.json')).read_text())
    if MODE == 'create':
        assert not unreal.EditorAssetLibrary.does_asset_exist(path)
        candidate = unreal.CSSAnimationLibrary.shift_loop(source, shift, path)
        assert candidate, label
        report = check(candidate, document, shift)
        assert unreal.EditorAssetLibrary.save_loaded_asset(candidate, False)
    else:
        candidate = unreal.load_asset(path)
        assert candidate
        report = check(candidate, document, shift)
    results.append(dict(clip=label, shift=shift, **report))
    (WORK/(MODE+'-phase-progress.json')).write_text(json.dumps(results, indent=2)+'\n')
    unreal.log('Phase verified '+label)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
(WORK/(MODE+'-phase-result.json')).write_text(json.dumps(dict(passed=True, clips=results,
    scope='Raw pose preservation at shifted source keys, not contact or gameplay acceptance.'), indent=2)+'\n')
runpy.run_path(str(Path(__file__).with_name('movement_eve.py')), run_name='__main__')

"""Fresh saved/compressed native reference check and unchanged Eve retarget control."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
REPO = ROOT / 'CustomShellSystem'
WORK = Path(os.environ['CSS_NATIVE_RETARGET_WORK']).resolve()
assert WORK.is_relative_to(REPO / 'work/cine-animation-port')
precise = os.environ.get('CSS_NATIVE_PRECISE_COMPRESSION') == '1'
prefix = 'precise-' if precise else ''
assert not (WORK / (prefix + 'verification.json')).exists()
creation = json.loads((WORK / 'result.json').read_text())
assert creation['reference_scale_enabled'] and creation['unaffected_max_error'] == 0
tools = unreal.AssetToolsHelpers.get_asset_tools()
target = unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_GameHuman1199_V1')
saved = unreal.load_asset('/Game/CSS/AnimLab/AN_NH1_BlackHole')
assert target and saved
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(target) == 9
clip = tools.duplicate_asset('AN_NH1_Compression', '/Game/CSSAuthoring/TransientProbes', saved)
assert clip
if precise:
    settings = unreal.load_asset('/Game/CSS/Locomotion/Studies/L5X/ACS_Motion')
    assert settings
    codecs = settings.get_editor_property('codecs')
    assert len(codecs) == 1 and abs(codecs[0].get_editor_property('ErrorThreshold') - .001) < 1e-9
    clip.set_editor_property('bone_compression_settings', settings)
assert unreal.CSSRetargetLibrary.prepare_compressed_animation(clip, target)
raw, compressed = unreal.AnimPoseEvaluationOptions(), unreal.AnimPoseEvaluationOptions()
for options, kind in ((raw, unreal.AnimDataEvalType.RAW), (compressed, unreal.AnimDataEvalType.COMPRESSED)):
    options.set_editor_property('evaluation_type', kind)
    options.set_editor_property('optional_skeletal_mesh', target)
    options.set_editor_property('should_retarget', True)
ref = {b['name']: b for b in json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target))}
errors = dict(position_cm=0.0, quaternion_distance=0.0, scale=0.0)
worst = {}
reference_scale_error = 0.0
for frame in range(creation['frames']):
    time = frame / creation['fps']
    a = unreal.AnimPoseExtensions.get_anim_pose_at_time(saved, time, raw)
    b = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, time, compressed)
    assert unreal.AnimPoseExtensions.is_valid(a) and unreal.AnimPoseExtensions.is_valid(b)
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(a)]
    assert names == [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(b)]
    assert len(names) == 1208
    for name in names:
        x = unreal.AnimPoseExtensions.get_bone_pose(a, name, unreal.AnimPoseSpaces.LOCAL)
        y = unreal.AnimPoseExtensions.get_bone_pose(b, name, unreal.AnimPoseSpaces.LOCAL)
        position = math.dist([x.translation.x, x.translation.y, x.translation.z],
                             [y.translation.x, y.translation.y, y.translation.z])
        qa, qb = [x.rotation.x, x.rotation.y, x.rotation.z, x.rotation.w], [y.rotation.x, y.rotation.y, y.rotation.z, y.rotation.w]
        rotation = min(math.dist(qa, qb), math.dist(qa, [-v for v in qb]))
        scale = math.dist([x.scale3d.x, x.scale3d.y, x.scale3d.z], [y.scale3d.x, y.scale3d.y, y.scale3d.z])
        assert all(math.isfinite(v) for v in (position, rotation, scale))
        for key, value in zip(errors, (position, rotation, scale), strict=True):
            if value > errors[key]:
                worst[key] = dict(frame=frame, bone=name)
            errors[key] = max(errors[key], value)
        if name in creation['affected_bones']:
            reference_scale_error = max(reference_scale_error,
                math.dist([y.scale3d.x, y.scale3d.y, y.scale3d.z], ref[name]['scale']))
    if frame % 100 == 0:
        (WORK / (prefix + 'verify-progress.json')).write_text(json.dumps(dict(frame=frame, frames=creation['frames'])))

source = unreal.load_asset('/Game/CSS/AnimLab/SK_L2_EveSource')
eve = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
source_clip = unreal.load_asset('/Game/CSS/AnimLab/AN_L2_S02A000')
retarget = unreal.load_asset('/Game/CSS/AnimLab/RT_L2_EveCSS')
previous = unreal.load_asset('/Game/CSS/AnimLab/RT_L2_S02A000')
assert source and eve and source_clip and retarget and previous
control = unreal.CSSAnimationLibrary.retarget_clip(source, eve, source_clip, retarget,
                                                  '/Game/CSS/AnimLab/AN_NH1_EveControl')
assert control
copy = tools.duplicate_asset('AN_EveControl', '/Game/CSS/Locomotion/Studies/NH1Check', control)
comparison = unreal.CSSAnimationLibrary.compare_sequence_data(previous, copy)
assert comparison.startswith('PASS:'), comparison
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in creation['protected'].items())
passed = errors['position_cm'] < .01 and errors['quaternion_distance'] < .001 and errors['scale'] < .00001 and reference_scale_error < .00001
report = dict(passed=passed, frames=creation['frames'], bones=1208, errors=errors,
              worst=worst, precise_compression=precise,
              reference_scale_error=reference_scale_error, eve_default_regression=comparison,
              protected_unchanged=True,
              scope='Fresh saved and editor compressed evaluation. Windows cook, real mesh and live graph remain pending.')
(WORK / (prefix + 'verification.json')).write_text(json.dumps(report, indent=2) + '\n')
assert passed, report

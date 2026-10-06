"""Stage a native-reference clip inside a required isolated game-skeleton mount."""
import gzip
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
REPO = ROOT / 'CustomShellSystem'
WORK = Path(os.environ['CSS_NATIVE_BINDING_WORK']).resolve()
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
assert WORK.is_relative_to(REPO / 'work/cine-animation-port')
assert not (WORK / 'stage-result.json').exists()
GAME = '/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton'
SOURCE = '/Game/CSS/AnimLab/AN_NH1_BlackHole'
TARGET = '/Game/CSS/Locomotion/NativeHuman/Studies/NH2/AN_BlackHole'
DIAGNOSTIC = '/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameHuman1199_V1'
SETTINGS = '/Game/CSS/Locomotion/Studies/L5X/ACS_Motion'


def disk(package):
    return CONTENT / (package.removeprefix('/Game/') + '.uasset')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


# The caller mounts this empty directory over the authoring placeholder only
# for this process. Never replace the ordinary project's game skeleton file.
mount = CONTENT / 'Sparta/Characters/Humans/_Shared'
marker = mount / 'css-native-reference-mount.txt'
assert marker.read_text().strip() == str(WORK)
assert not disk(GAME).exists() and not disk(TARGET).exists()
proof = json.loads((REPO / 'work/cine-animation-port/native-retarget-v1/precise-verification.json').read_text())
assert proof['passed'] and proof['reference_scale_error'] == 0
protected = {str(disk(p)): sha(disk(p)) for p in (SOURCE, DIAGNOSTIC, SETTINGS)}
tools = unreal.AssetToolsHelpers.get_asset_tools()
diagnostic = unreal.load_asset(DIAGNOSTIC)
source = unreal.load_asset(SOURCE)
settings = unreal.load_asset(SETTINGS)
assert diagnostic and source and settings
native = tools.duplicate_asset('SKEL_Human_Skeleton', '/Game/Sparta/Characters/Humans/_Shared', diagnostic)
assert native
assert unreal.CSSRetargetLibrary.inspect_skeleton(native) == unreal.CSSRetargetLibrary.inspect_skeleton(diagnostic)
assert unreal.CSSRetargetLibrary.inspect_virtual_bones(native) == unreal.CSSRetargetLibrary.inspect_virtual_bones(diagnostic)
assert unreal.EditorAssetLibrary.save_loaded_asset(native, False)
candidate = unreal.CSSAnimationLibrary.bind_native_human_clip(source, native, TARGET)
assert candidate, 'Exact-reference bind rejected'
assert candidate.get_editor_property('skeleton') == native
assert not candidate.get_editor_property('retarget_source_asset')
candidate.set_editor_property('bone_compression_settings', settings)
assert unreal.EditorAssetLibrary.save_loaded_asset(candidate, False)

options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('should_retarget', False)
options.set_editor_property('extract_root_motion', False)
reference = json.loads((REPO / 'work/cine-animation-port/native-retarget-v1/result.json').read_text())
frames, fps = reference['frames'], reference['fps']
with gzip.open(WORK / 'raw-reference.json.gz', 'wt') as stream:
    stream.write(json.dumps(dict(fps=fps, should_retarget=False))[:-1] + ', "frames": [')
    for frame in range(frames):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(candidate, frame/fps, options)
        assert unreal.AnimPoseExtensions.is_valid(pose)
        names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
        assert len(names) == 1208
        transforms = []
        for name in names:
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
            transforms.append(dict(Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                                   Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                                   Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z)))))
        if frame:
            stream.write(',')
        json.dump(dict(pose=dict(Snapshot=dict(bIsValid=True, BoneNames=names, LocalTransforms=transforms))), stream)
        if frame % 100 == 0:
            (WORK / 'progress.json').write_text(json.dumps(dict(frame=frame, frames=frames)))
    stream.write(']}')
assert all(sha(Path(p)) == checksum for p, checksum in protected.items())
report = dict(passed=True, package=TARGET, skeleton=GAME, frames=frames, fps=fps,
              duration=(frames-1)/fps, protected=protected,
              candidate_sha256=sha(disk(TARGET)), native_reference_sha256=sha(disk(GAME)),
              scope='Exact-reference binding and full raw capture. Cooked, visible-mesh and live checks pending.')
(WORK / 'stage-result.json').write_text(json.dumps(report, indent=2) + '\n')
(WORK / 'cook.txt').write_text(TARGET + '\n' + SETTINGS + '\n')

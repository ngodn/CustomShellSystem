"""Create private idle candidates with root-height edits only. UE 5.6.1."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
assert not (WORK/'import-result.json').exists()
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
protected = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in (content/'CSS').rglob('*.uasset')}
tools = unreal.AssetToolsHelpers.get_asset_tools()
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh', mesh)
results = []
settings_path = WORK/'import-settings.json'
settings = json.loads(settings_path.read_text()) if settings_path.exists() else {}
source_revision = settings.get('source', 'E4')
target_revision = settings.get('target', 'E5')
clips = settings.get('clips', list(range(700, 714)))
assert source_revision in ('E4', 'E5') and target_revision in ('E5', 'E6')
assert source_revision != target_revision and clips and len(clips) == len(set(clips))
assert all(clip in range(700, 714) for clip in clips)
for clip in clips:
    source = unreal.load_asset(f'/Game/CSS/AnimLab/RT_{source_revision}_Idle{clip}')
    path = f'/Game/CSS/AnimLab/RT_{target_revision}_Idle{clip}'
    assert source and not unreal.EditorAssetLibrary.does_asset_exist(path)
    animation = tools.duplicate_asset(f'RT_{target_revision}_Idle{clip}', '/Game/CSS/AnimLab', source)
    document = json.loads((WORK/f'idle{clip}-motion.json').read_text())
    snapshots = [f['pose']['Snapshot'] for f in document['frames']]
    root = snapshots[0]['BoneNames'].index('root')
    values = [s['LocalTransforms'][root] for s in snapshots]
    edit = animation.get_editor_property('controller')
    edit.open_bracket('Ground Eve idle pose', False)
    try:
        assert edit.set_bone_track_keys('root',
            [unreal.Vector(*[t['Translation'][k] for k in 'XYZ']) for t in values],
            [unreal.Quat(*[t['Rotation'][k] for k in 'XYZW']) for t in values],
            [unreal.Vector(*[t['Scale3D'][k] for k in 'XYZ']) for t in values], False)
    finally:
        edit.close_bracket(False)
    maximum = 0.
    for frame, expected in enumerate(snapshots):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(animation, frame/30, options)
        assert unreal.AnimPoseExtensions.is_valid(pose)
        for name, target in zip(expected['BoneNames'], expected['LocalTransforms'], strict=True):
            # Virtual bones are generated from the corrected physical bones.
            if name.startswith('VB '):
                continue
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
            error = math.dist([t.translation.x, t.translation.y, t.translation.z],
                              [target['Translation'][k] for k in 'XYZ'])
            assert error < .001, (clip, frame, name, error)
            q = [t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w]
            r = [target['Rotation'][k] for k in 'XYZW']
            assert min(math.dist(q, r), math.dist(q, [-v for v in r])) < .0001
            assert math.dist([t.scale3d.x, t.scale3d.y, t.scale3d.z],
                             [target['Scale3D'][k] for k in 'XYZ']) < .0001
            maximum = max(maximum, error)
    assert unreal.EditorAssetLibrary.save_loaded_asset(animation, False)
    results.append(dict(clip=clip, asset=animation.get_path_name(), frames=len(snapshots),
                        maximum_position_error_cm=maximum))
    (WORK/'import-progress.json').write_text(json.dumps(results, indent=2)+'\n')
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
(WORK/'import-result.json').write_text(json.dumps(dict(results=results,
    protected=protected, scope='Raw root-only edits; fresh cooked and gameplay validation pending'), indent=2)+'\n')

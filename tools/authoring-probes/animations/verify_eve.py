"""Fresh UE process: verify saved candidates and repeat the headless conversion."""
import hashlib
import json
import math
import os
import re
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ.get('CSS_ANIM_WORK', str(ROOT/'CustomShellSystem/work/anim2'))).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
config = json.loads((WORK/'batch-config.json').read_text()) if (WORK/'batch-config.json').exists() else {}
revision = config.get('revision', '')
assert re.fullmatch(r'[A-Za-z0-9]{0,8}', revision)
tag = revision+'_' if revision else ''
source_mesh = unreal.load_asset('/Game/CSS/AnimLab/SK_'+tag+'EveSource')
retarget = unreal.load_asset('/Game/CSS/AnimLab/RT_'+tag+'EveCSS')
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh', mesh)
results = []
batch = json.loads((WORK/'batch.json').read_text()) if (WORK/'batch.json').exists() else [('Walk', ''), ('Idle', '')]
for label, _ in batch:
    assert label in ('Walk', 'Idle', 'Jog', 'Sprint')
    saved = unreal.load_asset('/Game/CSS/AnimLab/RT_'+tag+label)
    source = unreal.load_asset('/Game/CSS/AnimLab/AN_'+tag+label)
    repeated = unreal.CSSAnimationLibrary.retarget_clip(
        source_mesh, mesh, source, retarget, '/Game/CSS/AnimLab/RT_'+tag+'Check'+label)
    assert saved and repeated
    document = json.loads((WORK/(label.lower()+'-motion.json')).read_text())
    for kind, animation in [('saved', saved), ('repeated', repeated)]:
        worst_position = worst_rotation = 0
        for frame, expected in enumerate(document['frames']):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(animation, frame/document['fps'], options)
            assert unreal.AnimPoseExtensions.is_valid(pose)
            names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
            snapshot = expected['pose']['Snapshot']
            assert names == snapshot['BoneNames']
            for name, value in zip(names, snapshot['LocalTransforms'], strict=True):
                t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
                position = math.dist([t.translation.x, t.translation.y, t.translation.z],
                                     [value['Translation'][k] for k in 'XYZ'])
                q = [t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w]
                r = [value['Rotation'][k] for k in 'XYZW']
                rotation = min(math.dist(q, r), math.dist(q, [-v for v in r]))
                assert position < .001 and rotation < .0001, (label, kind, frame, name, position, rotation)
                assert math.dist([t.scale3d.x, t.scale3d.y, t.scale3d.z],
                                 [value['Scale3D'][k] for k in 'XYZ']) < .0001
                worst_position = max(position, worst_position)
                worst_rotation = max(rotation, worst_rotation)
        results.append(dict(clip=label, source=kind, frames=len(document['frames']),
                            bones=len(names), max_position_cm=worst_position, max_quaternion_distance=worst_rotation))
protected = json.loads((WORK/'protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
(WORK/'readback-result.json').write_text(json.dumps(dict(passed=True, results=results,
    scope='Saved raw poses and repeated conversion in a fresh editor process. No candidate installed or cooked.'), indent=2)+'\n')

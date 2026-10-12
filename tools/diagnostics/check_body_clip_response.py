"""Compare body-rig response to retained animation poses without changing assets."""
import hashlib
import json
import math
import os
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare_body_rig_response import ROOT, STAGE, PACKAGES, NAMES, evaluate


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))
    candidate = os.environ.get('CSS_BODY_CANDIDATE')
    packages = {'current': candidate} if candidate else PACKAGES
    modes = ('current',) if candidate else ('current', 'current_old_limits', 'v42')
    clips = ['/Game/CSS/Locomotion/Eve/idle/AN_Default',
             '/Game/CSS/Locomotion/Studies/L5X/AN_IrisOut']
    paths = [content/(package.removeprefix('/Game/')+'.uasset')
             for package in list(packages.values())+clips]
    protected = {str(path): digest(path) for path in paths}
    public = json.loads((ROOT/'work/three-outfit-rig-physics/secondary-bindings5/report.json')
                        .read_text())['graphs']['/Game/CSS/eins0fx/EveSB/ABP_Secondary']['control_defaults']
    rigs = {name: unreal.load_asset(path) for name, path in packages.items()}
    if not all(isinstance(rig, unreal.ControlRigBlueprint) for rig in rigs.values()):
        raise ValueError('Missing retained rig')
    hierarchies = {name: rig.create_control_rig().get_hierarchy() for name, rig in rigs.items()}
    keys = {name: {str(key.name) for key in hierarchy.get_all_keys(True)
                   if key.type == unreal.RigElementType.BONE}
            for name, hierarchy in hierarchies.items()}
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('should_retarget', False)
    result = dict(scope='Raw animation poses into native rigs, no gameplay main AnimBP or component movement',
                  protected=protected, packages=packages, clips=[], cases=[])
    for package in clips:
        clip = unreal.load_asset(package)
        count = unreal.AnimationLibrary.get_num_keys(clip)
        rate = 30 if package.endswith('AN_Default') else 60
        if not math.isclose((count-1)/rate, clip.get_play_length(), abs_tol=1e-5):
            raise ValueError('Retained animation timing changed')
        frames, names = [], None
        for frame in range(count):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, frame/rate, options)
            if not unreal.AnimPoseExtensions.is_valid(pose):
                raise ValueError('Invalid animation pose')
            if names is None:
                names = {str(name) for name in unreal.AnimPoseExtensions.get_bone_names(pose)}
                if not set(NAMES).issubset(names):
                    raise ValueError('Animation omits body helpers')
            frames.append({name: unreal.AnimPoseExtensions.get_bone_pose(
                pose, name, unreal.AnimPoseSpaces.WORLD) for name in names.intersection(*keys.values())})
        result['clips'].append(dict(package=package, keys=len(frames), rate=rate,
                                    mapped_bones=len(frames[0]),
                                    unmapped_rig_bones={mode: sorted(bones-names) for mode, bones in keys.items()}))
        deltas = [0.]+[1/rate]*(len(frames)-1)
        for mode in modes:
            for preset in ('firm', 'natural', 'off'):
                row = evaluate(rigs['v42' if mode == 'v42' else 'current'],
                               public, mode, preset, rate, deltas, frames)
                row['clip'] = package
                result['cases'].append(row)
                (STAGE/'response.json').write_text(json.dumps(result)+'\n')
                unreal.log(f'Clip response complete: {package} {mode} {preset}')
    if any(digest(Path(path)) != expected for path, expected in protected.items()):
        raise ValueError('Source packages changed')
    result['inputs_unchanged'] = True
    (STAGE/'response.json').write_text(json.dumps(result)+'\n')


if __name__ == '__main__':
    main()

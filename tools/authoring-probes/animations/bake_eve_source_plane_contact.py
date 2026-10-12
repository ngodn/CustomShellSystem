"""Repeat the full L5S dance contact bake using the original knee plane."""
import gzip
import hashlib
import json
import os
import re
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
MODE = os.environ.get('CSS_CONTACT_MODE', 'create')
REVISION = os.environ.get('CSS_CONTACT_REVISION', 'L5U')
LABELS = os.environ.get('CSS_CONTACT_LABELS', 'DumbDumb,IrisOut').split(',')
BASE = WORK.parent / 'retarget-l5s'
if (not re.fullmatch(r'L[1-9][0-9]?[A-Z]', REVISION) or
        (WORK).resolve() != (ROOT / ('CustomShellSystem/work/cine-animation-port/retarget-' + REVISION.lower())).resolve() or
        MODE not in ('create', 'verify') or not LABELS or len(LABELS) != len(set(LABELS)) or
        not set(LABELS) <= {'DumbDumb', 'IrisOut'}):
    raise ValueError('Use an isolated numbered trial, audited clips and create/verify mode')
REPORT = WORK / (MODE + '-result.json')
if REPORT.exists():
    raise FileExistsError(REPORT)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def disk(package):
    if not package.startswith('/Game/') or '.' in package:
        raise ValueError('Expected a package path')
    return CONTENT / (package.removeprefix('/Game/') + '.uasset')


receipt = json.loads((BASE / 'result.json').read_text())
protected = dict(receipt['protected'])
protected[str(BASE / 'result.json')] = sha(BASE / 'result.json')
if any(sha(Path(p)) != h for p, h in protected.items()):
    raise ValueError('L5S input changed')
directions = None
if os.environ.get('CSS_KNEE_DIRECTIONS'):
    recipe = Path(os.environ['CSS_KNEE_DIRECTIONS']).resolve(strict=True)
    if recipe.parent != WORK:
        raise ValueError('Knee recipe must belong to the current isolated trial')
    directions = json.loads(recipe.read_text())
    if directions['clip'] not in LABELS:
        raise ValueError('Knee recipe does not match the selected clips')
    if any(sha(Path(p)) != h for p, h in directions['inputs'].items()):
        raise ValueError('Knee-direction input changed')
    protected.update(directions['inputs'])
    protected[str(recipe)] = sha(recipe)
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
if not mesh:
    raise ValueError('Missing audited target mesh')
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh', mesh)
options.set_editor_property('extract_root_motion', False)
options.set_editor_property('incorporate_root_motion_into_pose', True)
tools = unreal.AssetToolsHelpers.get_asset_tools()
results = []
for label in LABELS:
    original = unreal.load_asset('/Game/CSS/AnimLab/RT_L5S_' + label)
    clock = next(r for r in receipt['results'] if r['label'] == label)
    measurement = BASE / (label.lower() + '-perfoot-v1/report.json')
    contact = json.loads(measurement.read_text())
    motion = BASE / (label.lower() + '-motion.json')
    if (not original or not contact['footwear_only'] or not contact['measure_only'] or
            contact['fps'] != 60 or contact['motion_sha256'] != sha(motion)):
        raise ValueError('Missing or changed contact measurement')
    protected[str(measurement)] = sha(measurement)
    protected[str(motion)] = sha(motion)
    rows = contact['errors']
    if [r['frame'] for r in rows] != list(range(clock['frames'])):
        raise ValueError('Incomplete contact coverage')
    output = '/Game/CSS/AnimLab/RT_' + REVISION + '_' + label
    if MODE == 'create' and disk(output).exists():
        raise FileExistsError(output)
    arguments = (mesh, original, [r['foot_lowest_cm']['l'] for r in rows],
        [r['foot_lowest_cm']['r'] for r in rows], .1, output if MODE == 'create' else output + 'Check', True)
    if directions and directions['clip'] == label:
        candidate = unreal.CSSContactLibrary.bake_contact_with_knee_directions(*arguments,
            [unreal.Vector(*v) for v in directions['left']],
            [unreal.Vector(*v) for v in directions['right']])
    else:
        candidate = unreal.CSSContactLibrary.bake_sole_contact(*arguments)
    if not candidate:
        raise ValueError('Source-plane bake failed: ' + label)
    duration = unreal.AnimationLibrary.get_sequence_length(candidate)
    if abs(duration - clock['duration']) > .00002:
        raise ValueError('Duration changed')
    comparison = 'Fresh-process verification pending'
    if MODE == 'create':
        if not unreal.EditorAssetLibrary.save_loaded_asset(candidate, False):
            raise ValueError('Save failed')
    else:
        creation = json.loads((WORK / 'create-result.json').read_text())
        expected = next(r for r in creation['results'] if r['label'] == label)
        if sha(disk(output)) != expected['sha256']:
            raise ValueError('Saved candidate changed')
        saved = unreal.load_asset(output)
        copy = tools.duplicate_asset('AN_' + label, '/Game/CSS/Locomotion/Studies/' + REVISION + 'Check', candidate)
        comparison = unreal.CSSAnimationLibrary.compare_sequence_data(saved, copy)
        if not comparison.startswith('PASS:'):
            raise ValueError(comparison)
        with gzip.open(WORK / (label.lower() + '-motion.json.gz'), 'xt', compresslevel=3) as stream:
            stream.write('{"fps":60,"sampling":' + json.dumps('Saved ' + REVISION + ' raw playback at source keys') + ',"frames":[')
            for frame in range(clock['frames']):
                pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(saved, frame, options)
                if not unreal.AnimPoseExtensions.is_valid(pose):
                    raise ValueError('Invalid saved pose')
                names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
                transforms = []
                for name in names:
                    t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
                    transforms.append(dict(
                        Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                        Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                        Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z)))))
                if frame:
                    stream.write(',')
                json.dump(dict(pose=dict(Snapshot=dict(bIsValid=True,
                    SkeletalMeshName='SK_BlackPearl2', BoneNames=names, LocalTransforms=transforms))),
                    stream, separators=(',', ':'))
            stream.write(']}')
    results.append(dict(label=label, asset=output, frames=clock['frames'], fps=60,
        duration=duration, sha256=sha(disk(output)), saved_repeat_comparison=comparison))
    unreal.log(REVISION + ' source-plane contact: ' + label + ' ' + MODE)
if any(sha(Path(p)) != h for p, h in protected.items()):
    raise ValueError('Protected input changed')
REPORT.write_text(json.dumps(dict(complete=True, revision=REVISION, results=results, protected=protected,
    scope='Full L5S source-plane contact correction. Six leg tracks only; all other authored tracks checked exact by the native helper. Saved repeat, surface, knee, render, cook and gameplay acceptance are separate checks.'), indent=2) + '\n')

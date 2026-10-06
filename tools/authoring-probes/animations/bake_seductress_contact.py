"""Isolated older-Seductress contact trials using audited footwear visibility."""
import hashlib
import json
import os
import re
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
MODE = os.environ.get('CSS_CONTACT_MODE', 'create')
REVISION = os.environ.get('CSS_CONTACT_REVISION', 'S4')
KNEE_MODE = os.environ.get('CSS_CONTACT_KNEE_PLANE', 'goal')
if not re.fullmatch(r'S[1-9][0-9]{0,2}', REVISION) or KNEE_MODE not in ('goal', 'source'):
    raise ValueError('Expected numbered trial revision and goal/source knee plane')
if MODE not in ('create', 'verify'):
    raise ValueError('Expected create or verify mode')
REPORT = WORK / ('result.json' if MODE == 'create' else 'verify-result.json')
if not WORK.is_relative_to(ROOT / 'CustomShellSystem/work') or REPORT.exists():
    raise ValueError('Use a fresh isolated work directory')
contract = json.loads((WORK / 'footwear-contract.json').read_text())
protected = dict(contract['inputs'])


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load(path):
    file = CONTENT / (path.removeprefix('/Game/') + '.uasset')
    protected[str(file)] = digest(file)
    asset = unreal.load_asset(path)
    if not asset:
        raise ValueError('Cannot load ' + path)
    return asset


protected[str(WORK / 'footwear-contract.json')] = digest(WORK / 'footwear-contract.json')
if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Footwear evidence changed')
contact_path = WORK.parent / 'retarget-seductress-s3/surface-contact-by-material.json'
contact = json.loads(contact_path.read_text())
if not contact['complete'] or str(contact_path) not in protected:
    raise ValueError('Missing protected contact measurement')
protected.update(contact['inputs'])
if MODE == 'verify':
    creation = json.loads((WORK / 'result.json').read_text())
    if (not creation['complete'] or creation.get('revision', 'S4') != REVISION or
            creation.get('knee_mode', 'goal') != KNEE_MODE):
        raise ValueError('Incomplete contact bake')
    protected.update(creation['protected'])
if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Measured input changed')
mesh = load('/Game/CSSAuthoring/CSS_Seductress_eins0fx_P/SK_Seductress_MidasV5')
load('/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base')
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh', mesh)
options.set_editor_property('extract_root_motion', False)
results = []
for case in contact['results']:
    name = Path(case['motion']).name.removesuffix('-motion.json')
    if name not in ('BlackHole', 'EveCatwalk', 'ComeCloser'):
        raise ValueError('Unexpected audition')
    original = load('/Game/CSS/AnimLab/RT_S3_' + name)
    keys = unreal.AnimationLibrary.get_num_keys(original)
    if keys != case['frames']:
        raise ValueError('Contact measurement does not cover every source key')
    for mode, materials in contract['modes'].items():
        if mode not in ('Heels', 'Flat'):
            raise ValueError('Unexpected footwear mode')
        heights = []
        for side in ('l', 'r'):
            sections = case['sides'][side]['material_frame_min_z_cm']
            values = [sections[str(material)] for material in materials]
            if any(len(v) != keys for v in values):
                raise ValueError('Incomplete material contact samples')
            heights.append([min(v[frame] for v in values) for frame in range(keys)])
        label = name + mode
        output = '/Game/CSS/AnimLab/RT_' + REVISION + '_' + label
        candidate = unreal.CSSContactLibrary.bake_sole_contact(mesh, original, *heights, .1,
            output if MODE == 'create' else output + 'Check', KNEE_MODE == 'source')
        if not candidate:
            raise ValueError('Contact bake failed: ' + label)
        if MODE == 'create':
            if not unreal.EditorAssetLibrary.save_loaded_asset(candidate, False):
                raise ValueError('Cannot save contact trial: ' + label)
            comparison = 'Not yet reloaded'
        else:
            saved = load(output)
            expected = next(row for row in creation['results'] if row['asset'] == output)
            if digest(CONTENT / (output.removeprefix('/Game/') + '.uasset')) != expected['output_sha256']:
                raise ValueError('Saved contact trial changed')
            # The shared-pack comparator accepts only isolated Locomotion copies.
            # This temporary copy is never saved or included in a catalog.
            comparison_copy = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
                'AN_' + label, '/Game/CSS/Locomotion/Studies/Seductress' + REVISION + 'Check', candidate)
            if not comparison_copy:
                raise ValueError('Cannot prepare comparison copy')
            comparison = unreal.CSSAnimationLibrary.compare_sequence_data(saved, comparison_copy)
            if not comparison.startswith('PASS:'):
                raise ValueError('Saved and repeated contact trials differ: ' + comparison)
            candidate = saved
        frames = []
        samples = keys if MODE == 'create' else 2 * (keys - 1) + 1
        duration = unreal.AnimationLibrary.get_sequence_length(candidate)
        for frame in range(samples):
            pose = (unreal.AnimPoseExtensions.get_anim_pose_at_frame(candidate, frame, options)
                if MODE == 'create' else unreal.AnimPoseExtensions.get_anim_pose_at_time(
                    candidate, min(duration, duration * frame / (samples - 1)), options))
            if not unreal.AnimPoseExtensions.is_valid(pose):
                raise ValueError('Invalid contact pose')
            names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
            transforms = []
            for bone in names:
                t = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL)
                transforms.append(dict(Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                    Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                    Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z)))))
            frames.append(dict(pose=dict(Snapshot=dict(bIsValid=True,
                SkeletalMeshName=mesh.get_name(), BoneNames=names, LocalTransforms=transforms))))
        filename = label + ('-motion.json' if MODE == 'create' else '-dense-motion.json')
        with (WORK / filename).open('x') as stream:
            json.dump(dict(fps=(samples - 1) / duration,
                sampling='Authored keys' if MODE == 'create' else 'Saved raw playback at twice the source key rate',
                frames=frames), stream, separators=(',', ':'))
        results.append(dict(label=label, asset=output, frames=keys, fps=case['fps'], samples=samples,
            saved_repeat_comparison=comparison,
            duration=unreal.AnimationLibrary.get_sequence_length(candidate), materials=materials,
            output_sha256=digest(CONTENT / (output.removeprefix('/Game/') + '.uasset'))))
        (WORK / (MODE + '-progress.json')).write_text(json.dumps(results, indent=2) + '\n')
        unreal.log('Seductress contact trial saved: ' + label)
if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Protected input changed')
(REPORT).write_text(json.dumps(dict(complete=True, revision=REVISION, knee_mode=KNEE_MODE,
    results=results, protected=protected,
    scope=('Measured contact trials only. Six leg tracks changed; helper verifies every other track unchanged. '
           + ('Fresh saved/repeated authored-key comparison passed; dense samples read from saved clips. '
              if MODE == 'verify' else 'Fresh reload remains required. ')
           + 'Surface remeasurement, knee/velocity review, adapter/cook/live acceptance remain separate checks.')
), indent=2) + '\n')

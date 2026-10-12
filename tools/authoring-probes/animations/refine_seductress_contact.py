"""Refine measured between-key contacts, then sample saved playback twice as densely."""
import gzip
import hashlib
import json
import math
import os
import re
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
BASE = Path(os.environ.get('CSS_CONTACT_BASE', str(WORK.parent / 'retarget-seductress-s5'))).resolve(strict=True)
REVISION = os.environ.get('CSS_CONTACT_REVISION', 'S6')
CONTRACT = WORK.parent / 'retarget-seductress-s5/footwear-contract.json'
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
MODE = os.environ.get('CSS_CONTACT_MODE', 'create')
LABELS = os.environ.get('CSS_CONTACT_LABELS', 'ComeCloserFlat').split(',')
ALLOWED = {clip + mode for clip in ('BlackHole', 'EveCatwalk', 'ComeCloser') for mode in ('Heels', 'Flat')}
if (not WORK.resolve().is_relative_to((ROOT / 'CustomShellSystem/work').resolve()) or BASE.parent != WORK.parent or
        BASE == WORK or not re.fullmatch(r'S[1-9][0-9]{0,2}', REVISION) or MODE not in ('create', 'verify') or
        not LABELS or len(set(LABELS)) != len(LABELS) or not set(LABELS) <= ALLOWED):
    raise ValueError('Invalid trial directory, mode or labels')
REPORT = WORK / (MODE + '-result.json')
if REPORT.exists():
    raise FileExistsError(REPORT)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def asset_file(path):
    return CONTENT / (path.removeprefix('/Game/') + '.uasset')


verification = json.loads((BASE / 'verify-result.json').read_text())
measurement = json.loads((BASE / 'dense-surface-contact.json').read_text())
contract = json.loads(CONTRACT.read_text())
if not verification['complete'] or not measurement['complete']:
    raise ValueError('Source verification and dense measurement required')
protected = {**verification.get('protected', verification.get('inputs', {})), **measurement['inputs']}
protected[str(CONTRACT)] = sha(CONTRACT)
for name in ('verify-result.json', 'dense-surface-contact.json'):
    protected[str(BASE / name)] = sha(BASE / name)
if any(sha(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Input evidence changed')
mesh = unreal.load_asset('/Game/CSSAuthoring/CSS_Seductress_eins0fx_P/SK_Seductress_MidasV5')
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh', mesh)
options.set_editor_property('extract_root_motion', False)
options.set_editor_property('incorporate_root_motion_into_pose', True)
tools = unreal.AssetToolsHelpers.get_asset_tools()
results = []
for label in LABELS:
    row = next(r for r in verification['results'] if r['label'] == label)
    source = unreal.load_asset(row['asset'])
    dense_path = BASE / (label + '-dense-motion.json')
    if not dense_path.exists():
        dense_path = dense_path.with_suffix('.json.gz')
    opener = gzip.open if dense_path.suffix == '.gz' else open
    with opener(dense_path, 'rt') as stream:
        dense = json.load(stream)
    frames = len(dense['frames'])
    rate = round(dense['fps'])
    if rate not in (60, 120, 240) or abs(dense['fps'] - rate) > .0001:
        raise ValueError('Unexpected measured sampling rate')
    if sha(asset_file(row['asset'])) != row.get('output_sha256', row.get('sha256')):
        raise ValueError('Verified source animation changed')
    protected[str(asset_file(row['asset']))] = sha(asset_file(row['asset']))
    if str(dense_path) not in protected or sha(dense_path) != protected[str(dense_path)]:
        raise ValueError('Dense capture is not covered by measurement evidence')
    output = '/Game/CSS/AnimLab/RT_' + REVISION + '_' + label
    if MODE == 'create' and asset_file(output).exists():
        raise FileExistsError(output)
    sample = tools.duplicate_asset('AN_' + REVISION + '_Input' + label, '/Game/CSS/AnimLab', source)
    if not sample:
        raise ValueError('Cannot isolate resampled input')
    edit = sample.get_editor_property('controller')
    edit.open_bracket('Import measured playback without changing speed', False)
    try:
        edit.set_frame_rate(unreal.FrameRate(rate, 1), False)
        edit.set_number_of_frames(unreal.FrameNumber(frames - 1), False)
        order = dense['frames'][0]['pose']['Snapshot']['BoneNames']
        for index, name in enumerate(order):
            ts = [f['pose']['Snapshot']['LocalTransforms'][index] for f in dense['frames']]
            if not edit.set_bone_track_keys(name,
                    [unreal.Vector(*(t['Translation'][c] for c in 'XYZ')) for t in ts],
                    [unreal.Quat(*(t['Rotation'][c] for c in 'XYZW')) for t in ts],
                    [unreal.Vector(*(t['Scale3D'][c] for c in 'XYZ')) for t in ts], False):
                raise ValueError('Could not write resampled track: ' + name)
    finally:
        try:
            if not unreal.CSSAnimationLibrary.finalize_source_import(sample):
                raise ValueError('Cannot finalize dense sampling rate')
        finally:
            edit.close_bracket(False)
    timing = dict(expected_keys=frames, actual_keys=unreal.AnimationLibrary.get_num_keys(sample),
                  original_duration=row['duration'], duration=unreal.AnimationLibrary.get_sequence_length(sample))
    (WORK / (MODE + '-' + label + '-timing.json')).write_text(json.dumps(timing, indent=2))
    if (timing['actual_keys'] != frames or
            abs(timing['duration'] - row['duration']) > max(1e-6, row['duration'] * 1.2e-7)):
        raise ValueError('Resampling changed duration or key coverage: ' + str(timing))
    max_position = max_rotation = max_scale = max_quaternion_length_error = 0.0
    worst_rotation = None
    for index, name in enumerate(order):
        raw = json.loads(unreal.CSSContactLibrary.read_authored_track(sample, name))
        keys = raw['keys']
        if len(keys) != frames:
            raise ValueError('Missing authored keys: ' + name)
        for frame, t in enumerate(keys):
            original = dense['frames'][frame]['pose']['Snapshot']['LocalTransforms'][index]
            max_position = max(max_position, *(abs(t['Translation'][c] - original['Translation'][c]) for c in 'XYZ'))
            max_scale = max(max_scale, *(abs(t['Scale3D'][c] - original['Scale3D'][c]) for c in 'XYZ'))
            q = [t['Rotation'][c] for c in 'XYZW']
            p = [original['Rotation'][c] for c in 'XYZW']
            qlength, plength = math.sqrt(sum(v * v for v in q)), math.sqrt(sum(v * v for v in p))
            max_quaternion_length_error = max(max_quaternion_length_error, abs(qlength - 1), abs(plength - 1))
            if min(qlength, plength) <= 0 or max_quaternion_length_error > .0001:
                raise ValueError('Unexpected non-unit captured rotation')
            q = [v / qlength for v in q]
            p = [v / plength for v in p]
            rotation_error = min(max(abs(a - b) for a, b in zip(q, p)),
                                 max(abs(a + b) for a, b in zip(q, p)))
            if rotation_error > max_rotation:
                max_rotation = rotation_error
                worst_rotation = dict(bone=name, frame=frame, expected=p, actual=q, error=rotation_error)
    reference_error = 0.0
    if worst_rotation:
        expected = unreal.CSSContactLibrary.sequencer_rotation_reference(unreal.Quat(*worst_rotation['expected']))
        reference = [getattr(expected, c.lower()) for c in 'XYZW']
        reference_error = min(max(abs(a-b) for a,b in zip(reference,worst_rotation['actual'])),
                              max(abs(a+b) for a,b in zip(reference,worst_rotation['actual'])))
    (WORK / (MODE + '-' + label + '-resample.json')).write_text(json.dumps(dict(
        position_cm=max_position, rotation=max_rotation, scale=max_scale,
        quaternion_length_error=max_quaternion_length_error, worst_rotation=worst_rotation,
        model=raw['model'], worst_rotation_sequencer_reference_error=reference_error), indent=2))
    # This model stores float Euler channels even when the input contains quaternions.
    # Keep an explicit bound and independently check the worst observed conversion.
    allowed_rotation = .00003 if raw['model'] == 'AnimationSequencerDataModel' and reference_error < .000001 else .000001
    if max_position > .0001 or max_rotation > allowed_rotation or max_scale > .000001:
        raise ValueError(f'Resampling differs from saved playback: {max_position}, {max_rotation}, {max_scale}')
    case = next(r for r in measurement['results'] if Path(r['motion']).name == dense_path.name)
    footwear = 'Heels' if label.endswith('Heels') else 'Flat'
    heights = [[min(case['sides'][side]['material_frame_min_z_cm'][str(m)][i]
                    for m in contract['modes'][footwear]) for i in range(frames)] for side in ('l', 'r')]
    contact_input = tools.duplicate_asset('RT_' + REVISION + '_Input' + label, '/Game/CSS/AnimLab', sample)
    candidate = unreal.CSSContactLibrary.bake_sole_contact(mesh, contact_input, *heights, .1,
        output if MODE == 'create' else output + 'Check', True)
    if not candidate:
        raise ValueError('Dense contact bake failed')
    comparison = 'Fresh reload pending'
    if MODE == 'create':
        if not unreal.EditorAssetLibrary.save_loaded_asset(candidate, False):
            raise ValueError('Cannot save dense contact bake')
    else:
        creation = json.loads((WORK / 'create-result.json').read_text())
        expected = next(r for r in creation['results'] if r['label'] == label)
        if sha(asset_file(output)) != expected['sha256']:
            raise ValueError('Saved trial changed')
        saved = unreal.load_asset(output)
        copy = tools.duplicate_asset('AN_' + label, '/Game/CSS/Locomotion/Studies/Seductress' + REVISION + 'Check', candidate)
        comparison = unreal.CSSAnimationLibrary.compare_sequence_data(saved, copy)
        if not comparison.startswith('PASS:'):
            raise ValueError(comparison)
        candidate = saved
    if MODE == 'verify':
        count = 2 * (frames - 1) + 1
        duration = unreal.AnimationLibrary.get_sequence_length(candidate)
        # Stream the capture so higher sampling does not multiply peak memory or disk usage.
        with gzip.open(WORK / (label + '-dense-motion.json.gz'), 'xt', compresslevel=3) as stream:
            stream.write('{"fps":' + str((count - 1) / duration) + ',"sampling":' +
                json.dumps('Saved ' + REVISION + ' raw playback at ' + str(2 * rate) + ' Hz') + ',"frames":[')
            for frame in range(count):
                pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(candidate,
                    min(duration, duration * frame / (count - 1)), options)
                if not unreal.AnimPoseExtensions.is_valid(pose):
                    raise ValueError('Invalid refined pose')
                names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
                transforms = []
                for name in names:
                    t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
                    transforms.append(dict(Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                        Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                        Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z)))))
                if frame:
                    stream.write(',')
                json.dump(dict(pose=dict(Snapshot=dict(bIsValid=True, BoneNames=names,
                    LocalTransforms=transforms))), stream, separators=(',', ':'))
            stream.write(']}')
    results.append(dict(label=label, asset=output, frames=frames, duration=row['duration'],
        resample_max_position_cm=max_position, resample_max_quaternion_component=max_rotation,
        resample_max_quaternion_length_error=max_quaternion_length_error,
        resample_max_scale=max_scale, saved_repeat_comparison=comparison, sha256=sha(asset_file(output))))
    unreal.log('Dense contact trial: ' + label)
if any(sha(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Protected input changed')
REPORT.write_text(json.dumps(dict(complete=True, revision=REVISION, source=str(BASE), results=results, inputs=protected,
    scope='Isolated contact refinement, original speed. Surface, knee, rendered motion and live acceptance remain separate checks.'), indent=2) + '\n')

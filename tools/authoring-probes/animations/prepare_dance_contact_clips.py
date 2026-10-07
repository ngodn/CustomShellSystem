"""UE 5.6.1: bake measured per-foot clearance into isolated standing dances."""
import gzip
import hashlib
import json
import os
import re
from pathlib import Path

import unreal


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


root = Path(__file__).resolve().parents[3]
content = root.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
work = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
phase = os.environ.get('CSS_CONTACT_PHASE', 'prepare')
plan = json.loads((work / 'plan.json').read_text())
revision = plan['revision']
if (phase not in ('prepare', 'verify') or not re.fullmatch(r'Contact[1-9][0-9]?', revision) or
        (work / (phase + '-result.json')).exists()):
    raise ValueError('Use a fresh phase and an isolated Contact revision')
identities = {'css.eve.dance.' + name for name in (
    'bassdownlow', 'blackhole', 'comecloser', 'jangwonyoungspin')}
if len(plan['entries']) != 4 or {r['id'] for r in plan['entries']} != identities:
    raise ValueError('Expected the four reviewed standing dances')
protected = dict(plan['inputs'])
protected[str(work / 'plan.json')] = sha(work / 'plan.json')
for path, checksum in protected.items():
    if sha(Path(path)) != checksum:
        raise ValueError('Protected input changed: ' + path)
settings = unreal.load_asset('/Game/CSS/Locomotion/Studies/Precision1/ACS_Motion')
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
if not settings or not mesh:
    raise ValueError('Missing audited mesh or compression settings')
tools = unreal.AssetToolsHelpers.get_asset_tools()
results = []
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('should_retarget', False)
options.set_editor_property('extract_root_motion', False)
options.set_editor_property('incorporate_root_motion_into_pose', True)
catalog = json.loads((work / 'source-catalog.json').read_text())
selected = []
for row in plan['entries']:
    name = row['source'].split('.')[-1]
    if not name.startswith('AN_') or row['keys'] != round(row['duration'] * 30) + 1:
        raise ValueError('Unexpected source clock or name')
    package = '/Game/CSS/Locomotion/Studies/' + revision + '/' + name
    destination = content / (package.removeprefix('/Game/') + '.uasset')
    if phase == 'prepare' and destination.exists():
        raise FileExistsError(destination)
    source = unreal.load_asset(row['source'])
    if not source or abs(unreal.AnimationLibrary.get_sequence_length(source) - row['duration']) > .00001:
        raise ValueError('Missing source or changed clock')
    # The existing solver only accepts private AnimLab inputs. These copies are
    # unsaved; the final saved asset depends on neither temporary package.
    private = tools.duplicate_asset('RT_' + revision + '_' + name,
                                   '/Game/CSS/AnimLab', source)
    if not private or not unreal.CSSAnimationLibrary.compare_sequence_data(private, source).startswith('PASS:'):
        raise ValueError('Private source copy differs')
    arguments = (mesh, private, row['heights']['l'], row['heights']['r'], .1,
                 '/Game/CSS/AnimLab/RT_' + revision + '_Solved_' + name, True)
    if row.get('directions'):
        candidate = unreal.CSSContactLibrary.bake_contact_with_knee_directions(*arguments,
            [unreal.Vector(*v) for v in row['directions']['l']],
            [unreal.Vector(*v) for v in row['directions']['r']])
    else:
        candidate = unreal.CSSContactLibrary.bake_sole_contact(*arguments)
    if not candidate:
        raise ValueError('Contact solve failed: ' + name)
    copy_path = package + ('Check' if phase == 'verify' else '')
    copy = tools.duplicate_asset(copy_path.rsplit('/', 1)[1], copy_path.rsplit('/', 1)[0], candidate)
    if not copy or not unreal.CSSAnimationLibrary.compare_sequence_data(candidate, copy).startswith('PASS:'):
        raise ValueError('Final copy differs')
    copy.set_editor_property('bone_compression_settings', settings)
    comparison = 'Fresh-process verification pending'
    if phase == 'prepare':
        if not unreal.EditorAssetLibrary.save_loaded_asset(copy, False):
            raise ValueError('Could not save contact candidate')
    else:
        before = next(r for r in json.loads((work / 'prepare-result.json').read_text())['results']
                      if r['id'] == row['id'])
        if sha(destination) != before['sha256']:
            raise ValueError('Saved contact candidate changed')
        saved = unreal.load_asset(package)
        comparison = unreal.CSSAnimationLibrary.compare_sequence_data(saved, copy)
        if not comparison.startswith('PASS:'):
            raise ValueError(comparison)
        with gzip.open(work / (name + '-raw.json.gz'), 'xt', compresslevel=3) as stream:
            stream.write('{"fps":30,"should_retarget":false,"frames":[')
            for frame in range(row['keys']):
                pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(saved, frame, options)
                if not unreal.AnimPoseExtensions.is_valid(pose):
                    raise ValueError('Invalid saved pose')
                names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
                values = []
                for bone in names:
                    t = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL)
                    values.append(dict(
                        Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                        Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                        Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z)))))
                if frame:
                    stream.write(',')
                json.dump(dict(pose=dict(Snapshot=dict(bIsValid=True, SkeletalMeshName='SK_BlackPearl2',
                    BoneNames=names, LocalTransforms=values))), stream, separators=(',', ':'))
            stream.write(']}')
    result = dict(id=row['id'], asset=package + '.' + name, keys=row['keys'], fps=30,
                  duration=row['duration'], sha256=sha(destination), result=comparison)
    results.append(result)
    entry = next(e for e in catalog['animation_library']['entries'] if e['id'] == row['id'])
    if len(entry['assets']) != 1:
        raise ValueError('Ambiguous catalog entry')
    entry['assets'][0]['asset'] = result['asset']
    selected.append(entry)
    print('CONTACT_CORRECTION', phase, name, comparison, flush=True)
for path, checksum in protected.items():
    if sha(Path(path)) != checksum:
        raise ValueError('Protected input changed: ' + path)
for result in results:
    path = content / (result['asset'].split('.')[0].removeprefix('/Game/') + '.uasset')
    protected[str(path)] = result['sha256']
(work / (phase + '-result.json')).write_text(json.dumps(dict(
    complete=True, results=results, protected=protected, installed=False,
    scope='Per-foot clearance at original 30 Hz keys. Six leg tracks only; native solver verifies '
          'exact preservation of all other authored tracks. Surface, cooked and live checks remain separate.'
), indent=2) + '\n')
if phase == 'prepare':
    catalog['animation_library']['entries'] = selected
    # This small cook probe contains sequences only. The combined-pack builder
    # retains the original adapters and facial resources when replacing them.
    catalog['animation_library'].pop('adapters', None)
    catalog['animation_library'].pop('faces', None)
    (work / 'catalog.json').write_text(json.dumps(catalog, indent=2) + '\n')
    (work / 'cook.txt').write_text(''.join(r['asset'].split('.')[0] + '\n' for r in results))

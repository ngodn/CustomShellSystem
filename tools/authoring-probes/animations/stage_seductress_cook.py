"""Stage the verified S8 audition without its editor-only preview mesh reference."""
import hashlib
import json
import os
from pathlib import Path

import unreal


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


root = Path(__file__).resolve().parents[3]
work = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
mode = os.environ['CSS_ANIM_MODE']
if not work.resolve().is_relative_to((root / 'work/cine-animation-port').resolve()) or mode not in ('create', 'verify'):
    raise ValueError('Use an isolated animation workspace and create/verify mode')
content = root.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
receipt_path = root / 'work/cine-animation-port/retarget-seductress-s8/verify-result.json'
receipt = json.loads(receipt_path.read_text())
if not receipt['complete'] or len(receipt['results']) != 1:
    raise ValueError('S8 fresh readback is incomplete')
row = receipt['results'][0]
if row['label'] != 'ComeCloserFlat' or not row['saved_repeat_comparison'].startswith('PASS:'):
    raise ValueError('Unexpected S8 source or comparison')
protected = dict(receipt['inputs'])
protected[str(receipt_path)] = digest(receipt_path)


def disk(package):
    return content / (package.removeprefix('/Game/') + '.uasset')


source = unreal.load_asset(row['asset'])
if not source or digest(disk(row['asset'])) != row['sha256']:
    raise ValueError('Source animation changed')
protected[str(disk(row['asset']))] = row['sha256']
target = '/Game/CSS/Locomotion/Studies/S8/AN_ComeCloserFlat'
settings_path = '/Game/CSS/Locomotion/Studies/S8/ACS_Motion'
if any(digest(Path(p)) != sha for p, sha in protected.items()):
    raise ValueError('Source verification input changed')
if mode == 'create':
    if disk(target).exists() or disk(settings_path).exists():
        raise FileExistsError('Study candidate already exists')
    settings = unreal.EditorAssetLibrary.duplicate_asset('/ACLPlugin/ACLAnimBoneCompressionSettings', settings_path)
    candidate = unreal.EditorAssetLibrary.duplicate_asset(row['asset'], target)
    if not settings or not candidate:
        raise ValueError('Could not create isolated candidate')
    codecs = settings.get_editor_property('codecs')
    if len(codecs) != 1 or not codecs[0].get_path_name().startswith(settings_path + '.'):
        raise ValueError('Unexpected codec ownership')
    codecs[0].set_editor_property('ErrorThreshold', .001)
    candidate.set_editor_property('bone_compression_settings', settings)
    candidate.set_preview_skeletal_mesh(None)
else:
    protected.update({str(disk(p)): digest(disk(p)) for p in (target, settings_path)})
    candidate = unreal.load_asset(target)
    settings = unreal.load_asset(settings_path)
    if not candidate or not settings:
        raise ValueError('Saved candidate is missing')
comparison = unreal.CSSAnimationLibrary.compare_sequence_data(source, candidate)
if not comparison.startswith('PASS:'):
    raise ValueError(comparison)
if mode == 'create':
    for asset in (settings, candidate):
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            raise ValueError('Candidate save failed')
if any(digest(Path(p)) != sha for p, sha in protected.items()):
    raise ValueError('Protected source changed')
protected.update({str(disk(p)): digest(disk(p)) for p in (target, settings_path)})
entry = dict(id='css.study.s8.comecloserflat', asset=target + '.AN_ComeCloserFlat',
             keys=row['frames'], duration=row['duration'], hz=240, result=comparison)
(work / (mode + '-result.json')).write_text(json.dumps(dict(
    complete=True, results=[entry], protected=protected, installed=False), indent=2) + '\n')
if mode == 'verify':
    (work / 'cook.txt').write_text(target + '\n')
    (work / 'compression-cook.txt').write_text(settings_path + '\n')
    (work / 'catalog.json').write_text(json.dumps(dict(animation_library=dict(
        id='css.study.s8', name='Seductress contact study', author='eins0fx', version='0.0.0-dev',
        entries=[dict(id=entry['id'], name='Come Closer Flat contact study', playback='once',
                      assets=[dict(asset=entry['asset'])])])), indent=2) + '\n')
print('S8_COOK_STUDY', mode, comparison, flush=True)

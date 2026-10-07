"""Make isolated compression-only copies of verified shared animations."""
import copy
import hashlib
import json
import os
from pathlib import Path

import unreal


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


work = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
config = json.loads((work / 'precision-plan.json').read_text())
root = Path(__file__).resolve().parents[3]
content = root.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
reference = Path(config['reference']).resolve(strict=True)
receipt = json.loads(reference.read_text())
if receipt['complete'] is not True or not 1 <= len(config['ids']) <= 8:
    raise ValueError('Expected verified authoring data and a small clip selection')
rows = [row for row in receipt['results'] if row['id'] in config['ids']]
if len(rows) != len(set(config['ids'])) or len(rows) != len(config['ids']):
    raise ValueError('Missing or duplicate source identity')
prefix = config['destination']
if (not prefix.startswith('/Game/CSS/Locomotion/Studies/') or '..' in prefix or
        '.' in prefix or prefix.endswith('/')):
    raise ValueError('Expected an isolated shared study namespace')
protected = dict(receipt['protected'])
protected[str(reference)] = sha(reference)
for path, expected in protected.items():
    if sha(Path(path)) != expected:
        raise ValueError('Protected input changed: ' + path)
settings_path = prefix + '/ACS_Motion'
phase = os.environ.get('CSS_PRECISION_PHASE', 'prepare')
if phase not in ('prepare', 'verify'):
    raise ValueError('Unknown precision phase')
if phase == 'prepare':
    if (content / prefix.removeprefix('/Game/')).exists():
        raise FileExistsError(prefix)
    original = unreal.load_asset('/ACLPlugin/ACLAnimBoneCompressionSettings')
    if not original:
        raise ValueError('Missing ACL settings')
    source_codecs = original.get_editor_property('codecs')
    thresholds = [codec.get_editor_property('ErrorThreshold') for codec in source_codecs]
    settings = unreal.EditorAssetLibrary.duplicate_asset(original.get_path_name(), settings_path)
    codecs = settings.get_editor_property('codecs') if settings else []
    if len(codecs) != 1 or not codecs[0].get_path_name().startswith(settings_path + '.'):
        raise ValueError('Compression codec must belong to the new settings')
    codecs[0].set_editor_property('ErrorThreshold', .001)
    if [codec.get_editor_property('ErrorThreshold') for codec in source_codecs] != thresholds:
        raise ValueError('Original codec changed')
    if not unreal.EditorAssetLibrary.save_loaded_asset(settings, False):
        raise ValueError('Could not save private settings')
else:
    settings = unreal.load_asset(settings_path)
if not settings or abs(settings.get_editor_property('codecs')[0].get_editor_property('ErrorThreshold') - .001) > 1e-9:
    raise ValueError('Unexpected compression setting')
results, packages = [], []
catalog = json.loads(Path(config['catalog']).read_text())
entries = []
for row in rows:
    source_path = content / (row['asset'].split('.')[0].removeprefix('/Game/') + '.uasset')
    if sha(source_path) != row['sha256']:
        raise ValueError('Verified source changed')
    source = unreal.load_asset(row['asset'])
    package = prefix + '/' + row['asset'].split('.')[-1]
    candidate = (unreal.EditorAssetLibrary.duplicate_asset(row['asset'], package)
                 if phase == 'prepare' else unreal.load_asset(package))
    if not source or not candidate:
        raise ValueError('Could not load the source and its private copy')
    if phase == 'prepare':
        candidate.set_editor_property('bone_compression_settings', settings)
        if not unreal.EditorAssetLibrary.save_loaded_asset(candidate, False):
            raise ValueError('Could not save private sequence')
    if candidate.get_editor_property('bone_compression_settings') != settings:
        raise ValueError('Saved sequence uses the wrong compression setting')
    comparison = unreal.CSSAnimationLibrary.compare_sequence_data(source, candidate)
    if not comparison.startswith('PASS:'):
        raise ValueError(comparison)
    result = copy.deepcopy(row)
    result.update(asset=candidate.get_path_name(), result=comparison,
                  sha256=sha(content / (package.removeprefix('/Game/') + '.uasset')))
    results.append(result)
    packages.append(package)
    entry = copy.deepcopy(next(e for e in catalog['animation_library']['entries'] if e['id'] == row['id']))
    if len(entry['assets']) != 1 or entry['assets'][0]['asset'] != row['asset']:
        raise ValueError('Unexpected source asset binding')
    entry['assets'][0]['asset'] = candidate.get_path_name()
    entries.append(entry)
    print('PRECISION_COPY', row['id'], comparison, flush=True)
for path, expected in protected.items():
    if sha(Path(path)) != expected:
        raise ValueError('Protected input changed: ' + path)
for package in packages + [settings_path]:
    path = content / (package.removeprefix('/Game/') + '.uasset')
    protected[str(path)] = sha(path)
result = dict(complete=True, results=results, protected=protected, installed=False,
              scope='Compression-only copies. Exact authored sequence data retained.')
(work / ('verify-result.json' if phase == 'verify' else 'prepare-result.json')).write_text(json.dumps(result, indent=2) + '\n')
if phase == 'prepare':
    library = dict(id='css.locomotion.precision-study', name='Shared compression study',
                   version='0.0.0-dev', author='eins0fx', entries=entries)
    (work / 'catalog.json').write_text(json.dumps(dict(animation_library=library), indent=2) + '\n')
    (work / 'cook.txt').write_text('\n'.join(packages) + '\n')
    (work / 'compression-cook.txt').write_text(settings_path + '\n')

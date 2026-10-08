"""Tighten compression on the demonstrated native Cinderella candidate only."""
import hashlib
import json
import os
from pathlib import Path

import unreal

work = Path(os.environ['CSS_NATIVE_LIBRARY_WORK']).resolve(strict=True)
plan = json.loads(Path(os.environ['CSS_NATIVE_LIBRARY_PLAN']).read_text())
creation = json.loads(Path(os.environ['CSS_NATIVE_CREATION_REPORT']).read_text())
rows = [r for r in creation['clips'] if r['id'] == 'css.eve.dance.cinderella']
if len(rows) != 1:
    raise ValueError('Expected one Cinderella candidate')
row = rows[0]
content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve()
if content != Path(plan['content']).resolve():
    raise ValueError('Wrong authoring project')


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


source = unreal.load_asset(row['package'])
settings = unreal.load_asset(plan['compression'])
if not source or not settings:
    raise ValueError('Missing precision inputs')
snapshot = unreal.CSSRigAuditLibrary.snapshot_animation(source)
if json.loads(snapshot) != json.loads(Path(row['snapshot']).read_text()):
    raise ValueError('Authored motion changed')
tools = unreal.AssetToolsHelpers.get_asset_tools()
directory = '/Game/CSS/eins0fx/Shared/Locomotion/NativeHuman/Studies/C12'
for name in ('ACS_Precise', 'AN_Cinderella'):
    if unreal.EditorAssetLibrary.does_asset_exist(directory + '/' + name):
        raise ValueError('Precision candidate already exists')
codec = tools.duplicate_asset('ACS_Precise', directory, settings)
codecs = codec.get_editor_property('codecs')
if len(codecs) != 1:
    raise ValueError('Unexpected compression settings')
codecs[0].set_editor_property('ErrorThreshold', .0001)
if abs(settings.get_editor_property('codecs')[0].get_editor_property('ErrorThreshold') - .001) > 1e-9:
    raise ValueError('Source codec changed')
candidate = tools.duplicate_asset('AN_Cinderella', directory, source)
candidate.set_editor_property('bone_compression_settings', codec)
if unreal.CSSRigAuditLibrary.snapshot_animation(candidate) != snapshot:
    raise ValueError('Precision change modified authored motion')
for asset in (codec, candidate):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError('Cannot save precision candidate')
output = directory + '/AN_Cinderella'
result = dict(row, package=output, sha256=sha(content / (output.removeprefix('/Game/') + '.uasset')))
for path, expected in plan['protected'].items():
    if sha(path) != expected:
        raise ValueError('Protected input changed')
if sha(content / (row['package'].removeprefix('/Game/') + '.uasset')) != row['sha256']:
    raise ValueError('Original candidate changed')
(work / 'result.json').write_text(json.dumps(dict(authored=True, clips=[result],
    compression=dict(package=directory+'/ACS_Precise', error_threshold=.0001),
    original_candidate_unchanged=True), indent=2) + '\n')

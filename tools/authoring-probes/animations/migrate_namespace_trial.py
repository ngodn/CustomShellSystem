"""UE 5.6.1: rename a staged asset group and compare its animation data."""
import hashlib
import json
import os
from pathlib import Path

import unreal


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def snapshot(asset):
    result = {'class': asset.get_class().get_name()}
    if isinstance(asset, unreal.AnimSequence):
        raw = unreal.CSSRigAuditLibrary.snapshot_animation(asset)
        if raw.startswith('ERROR:'):
            raise ValueError(raw + ': ' + asset.get_path_name())
        result.update(
            source=json.loads(raw),
            rate_scale=asset.get_editor_property('rate_scale'),
            root_motion=asset.get_editor_property('enable_root_motion'),
            root_lock=str(asset.get_editor_property('root_motion_root_lock')))
    elif isinstance(asset, unreal.BlendSpace):
        result['samples'] = []
        for sample in asset.get_editor_property('sample_data'):
            animation = sample.get_editor_property('animation')
            result['samples'].append(dict(animation=animation.get_name() if animation else None,
                value=sample.get_editor_property('sample_value').export_text(),
                rate_scale=sample.get_editor_property('rate_scale')))
    elif isinstance(asset, unreal.Skeleton):
        raw = unreal.CSSRigAuditLibrary.snapshot_skeleton(asset)
        if raw.startswith('ERROR:'):
            raise ValueError(raw)
        result['source'] = json.loads(raw)
    elif isinstance(asset, (unreal.AnimBlueprint, unreal.AnimBoneCompressionSettings,
                            unreal.MaterialInterface, unreal.Texture)):
        raw = unreal.CSSRigAuditLibrary.snapshot_asset_properties(asset)
        if raw.startswith('ERROR:'):
            raise ValueError(raw)
        result['properties'] = json.loads(raw)
    else:
        raise ValueError('Unsupported migration asset class: ' + result['class'])
    return result


def references(asset):
    result = {}
    properties = []
    if isinstance(asset, (unreal.AnimSequence, unreal.BlendSpace)):
        properties.append('skeleton')
    if isinstance(asset, unreal.AnimBlueprint):
        properties.append('target_skeleton')
    if isinstance(asset, unreal.AnimSequence):
        properties += ['retarget_source_asset', 'bone_compression_settings', 'curve_compression_settings']
    for name in properties:
        value = asset.get_editor_property(name)
        result[name] = value.get_path_name() if value else None
    if isinstance(asset, unreal.BlendSpace):
        result['samples'] = [sample.get_editor_property('animation').get_path_name()
                             if sample.get_editor_property('animation') else None
                             for sample in asset.get_editor_property('sample_data')]
    return result


def mapped_references(value, mapping):
    if isinstance(value, dict):
        return {key: mapped_references(item, mapping) for key, item in value.items()}
    if isinstance(value, list):
        return [mapped_references(item, mapping) for item in value]
    if isinstance(value, str):
        package, separator, obj = value.partition('.')
        if package in mapping:
            if not separator or obj != package.rsplit('/', 1)[1]:
                raise ValueError('Unexpected reference object: ' + value)
            target = mapping[package]
            return target + '.' + target.rsplit('/', 1)[1]
    return value


def normalized_properties(value, mapping):
    if isinstance(value, dict):
        return {normalized_properties(key, mapping): normalized_properties(item, mapping)
                for key, item in value.items()}
    if isinstance(value, list):
        return [normalized_properties(item, mapping) for item in value]
    if isinstance(value, str):
        for new, old in sorted(mapping.items(), key=lambda pair: len(pair[0]), reverse=True):
            value = value.replace(new, old)
    return value


def verify_references(asset, expected, exact=None):
    if exact is not None:
        if references(asset) != exact:
            raise ValueError('Migrated references differ: ' + asset.get_path_name())
        return
    if isinstance(asset, (unreal.AnimSequence, unreal.BlendSpace)):
        skeleton = asset.get_editor_property('skeleton').get_path_name().split('.')[0]
        if skeleton != '/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base':
            raise ValueError('Unmigrated skeleton reference: ' + skeleton)
    if isinstance(asset, unreal.BlendSpace):
        animations = [s.get_editor_property('animation') for s in asset.get_editor_property('sample_data')]
        if any(a and a.get_path_name().split('.')[0] not in expected for a in animations):
            raise ValueError('Unmigrated blend sample: ' + asset.get_path_name())


def main():
    stage = Path(os.environ['CSS_NAMESPACE_STAGE']).resolve(strict=True)
    content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve()
    if content != stage / 'Content' or not unreal.SystemLibrary.get_engine_version().startswith('5.6.1-'):
        raise ValueError('Expected the isolated UE 5.6.1 staging project')
    plan = json.loads((stage / 'input.json').read_text())
    if 'native_reference' in plan:
        native = plan['native_reference']
        native_path = content / (native['package'].removeprefix('/Game/') + '.uasset')
        if digest(native_path) != native['sha256'] or digest(Path(native['source'])) != native['sha256']:
            raise ValueError('Native skeleton reference was replaced or modified')
        native_asset = unreal.load_asset(native['package'])
        native_snapshot = snapshot(native_asset)
        if os.environ.get('CSS_NAMESPACE_VERIFY') == '1':
            if native_snapshot != json.loads((stage / 'native-reference.json').read_text()):
                raise ValueError('Reloaded native skeleton differs')
        else:
            (stage / 'native-reference.json').write_text(json.dumps(native_snapshot, indent=2)+'\n')
    report_path = stage / 'migration.json'
    expected = {e['new_package'] for e in plan['assets']}
    if os.environ.get('CSS_NAMESPACE_VERIFY') == '1':
        if any((content / (e['old_package'].removeprefix('/Game/') + '.uasset')).exists()
               for e in plan['assets']):
            raise ValueError('Remove staged legacy paths from Content before the reload check')
        report = json.loads(report_path.read_text())
        for entry in report['assets']:
            package = entry['package']
            path = content / (package.removeprefix('/Game/') + '.uasset')
            if digest(path) != entry['sha256']:
                raise ValueError('Saved candidate changed: ' + package)
            asset = unreal.load_asset(package)
            if asset is None or snapshot(asset) != entry['snapshot']:
                raise ValueError('Reloaded source data differs: ' + package)
            verify_references(asset, expected, entry.get('references'))
        if any(digest(Path(e['source_file'])) != e['source_sha256'] for e in plan['assets']):
            raise ValueError('Original input changed')
        (stage / 'reload.json').write_text(json.dumps(dict(passed=True, assets=len(report['assets']),
            legacy_redirectors_present=False, original_sources_unchanged=True,
            scope='Fresh editor process, source-data and internal animation references; cook/game pending'), indent=2)+'\n')
        print('CSS_NAMESPACE_RELOAD_PASS', len(report['assets']), flush=True)
        return
    if report_path.exists():
        raise FileExistsError(report_path)
    for entry in plan['assets']:
        source = Path(entry['source_file'])
        copied = content / (entry['old_package'].removeprefix('/Game/') + '.uasset')
        if source == copied or copied.is_symlink() or digest(source) != entry['source_sha256'] or digest(copied) != entry['source_sha256']:
            raise ValueError('Source or staging identity changed: ' + str(source))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    loaded, before, renames, refreshed, before_refs = {}, {}, [], [], {}
    mapping = {entry['old_package']: entry['new_package'] for entry in plan['assets']}
    for entry in plan['assets']:
        old, new = entry['old_package'], entry['new_package']
        if unreal.EditorAssetLibrary.does_asset_exist(new):
            raise FileExistsError(new)
        asset = unreal.load_asset(old)
        if asset is None:
            raise ValueError('Could not load ' + old)
        loaded[old] = asset
        if (plan.get('refresh_retarget_reference_pose') or old in plan.get('refresh_packages', [])) and isinstance(asset, unreal.AnimSequence):
            source_mesh = asset.get_editor_property('retarget_source_asset')
            if not isinstance(source_mesh, unreal.SkeletalMesh):
                raise ValueError('Missing retarget reference mesh: ' + old)
            initial = snapshot(asset)
            asset.update_retarget_source_asset_data()
            updated = snapshot(asset)
            initial_pose = initial['source'].pop('retarget_reference_pose')
            updated_pose = updated['source'].pop('retarget_reference_pose')
            if initial != updated or not updated_pose:
                raise ValueError('Retarget refresh changed animation keys or metadata: ' + old)
            refreshed.append(dict(package=old, source_mesh=source_mesh.get_path_name(),
                                  before=initial_pose, after=updated_pose))
        before[old] = snapshot(asset)
        before_refs[old] = references(asset)
        folder, name = new.rsplit('/', 1)
        renames.append(unreal.AssetRenameData(asset, folder, name))
    (stage / 'before.json').write_text(json.dumps(before, indent=2) + '\n')
    (stage / 'before-references.json').write_text(json.dumps(before_refs, indent=2) + '\n')
    (stage / 'retarget-refresh.json').write_text(json.dumps(refreshed, indent=2) + '\n')
    if not unreal.AssetToolsHelpers.get_asset_tools().rename_assets(renames):
        raise RuntimeError('Unreal asset rename failed; retain the staging log')
    changed = []
    for entry in plan['assets']:
        old, new = entry['old_package'], entry['new_package']
        asset = loaded[old]
        current = snapshot(asset)
        compared = normalized_properties(current, {new: old for old, new in mapping.items()}) if 'properties' in current else current
        if asset.get_path_name().split('.')[0] != new or compared != before[old]:
            (stage / 'snapshot-mismatch.json').write_text(json.dumps(dict(package=old, before=before[old], after=compared), indent=2)+'\n')
            raise ValueError('Unexpected source-data change: ' + old)
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            raise RuntimeError('Could not save ' + new)
        exact = mapped_references(before_refs[old], mapping)
        verify_references(asset, expected, exact)
        path = content / (new.removeprefix('/Game/') + '.uasset')
        changed.append(dict(package=new, sha256=digest(path), snapshot=snapshot(asset), references=exact))
        print('CSS_NAMESPACE_SAVED', len(changed), len(plan['assets']), new, flush=True)
    if any(digest(Path(e['source_file'])) != e['source_sha256'] for e in plan['assets']):
        raise ValueError('An original source changed')
    report_path.write_text(json.dumps(dict(assets=changed, original_sources_unchanged=True,
        scope='Isolated editor save and in-memory animation comparison; reload/cook pending'), indent=2)+'\n')
    print('CSS_NAMESPACE_RENAME_PASS', len(changed), flush=True)


if __name__ == '__main__':
    main()

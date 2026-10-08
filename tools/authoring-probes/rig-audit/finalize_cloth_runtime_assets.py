"""Rename verified private cloth candidates into their final package paths in an isolated project."""
import hashlib
import json
import os
from pathlib import Path

import unreal


stage = Path(os.environ['CSS_PANEL_STAGE']).resolve(strict=True)
plan = json.loads((stage / 'finalize-plan.json').read_text())
verify = os.environ.get('CSS_PANEL_VERIFY_FINAL') == '1'
mapping = plan['packages']
loaded = {}


def decode(text):
    if text.startswith('ERROR:'):
        raise RuntimeError(text)
    return json.loads(text)


def snapshot(asset):
    if isinstance(asset, unreal.SkeletalMesh):
        mesh = decode(unreal.CSSRigAuditLibrary.snapshot_mesh(asset))
        morphs = [v['name'] for v in mesh['morphs']]
        return dict(mesh=mesh, render=decode(unreal.CSSRigAuditLibrary.snapshot_render_sections(
            asset, [], morphs, True)), cloth=decode(unreal.CSSRigAuditLibrary.snapshot_cloth_custom_data(asset)),
            default=asset.get_default_mesh_deformer().get_path_name(),
            collection=asset.get_target_mesh_deformers().get_path_name())
    if isinstance(asset, unreal.MeshDeformerCollection):
        return dict(graphs=[v.get_path_name() for v in asset.get_editor_property('mesh_deformers')])
    return decode(unreal.CSSRigAuditLibrary.snapshot_deformer(asset))


def normalize(value):
    text = json.dumps(value, sort_keys=True)
    replacements = {}
    for i, (old, new) in enumerate(sorted(mapping.items(), key=lambda pair: len(pair[1]), reverse=True)):
        object_token, package_token = f'__CSS_OBJECT_{i}__', f'__CSS_PACKAGE_{i}__'
        text = text.replace(new + '.' + new.rsplit('/', 1)[1], object_token)
        text = text.replace(new, package_token)
        replacements[object_token] = old + '.' + old.rsplit('/', 1)[1]
        replacements[package_token] = old
    for token, source in replacements.items():
        text = text.replace(token, source)
    return json.loads(text)


for old, new in mapping.items():
    loaded[old] = unreal.load_asset(new if verify else old)
    assert loaded[old], old
if verify:
    before = json.loads((stage / 'before.json').read_text())
else:
    before = {old: snapshot(asset) for old, asset in loaded.items()}
    (stage / 'before.json').write_text(json.dumps(before, indent=2))
    renames = [unreal.AssetRenameData(loaded[old], new.rsplit('/', 1)[0], new.rsplit('/', 1)[1])
               for old, new in mapping.items()]
    assert unreal.AssetToolsHelpers.get_asset_tools().rename_assets(renames)
    graph, = [asset for asset in loaded.values() if isinstance(asset, unreal.OptimusDeformer)]
    assert unreal.CSSRigAuditLibrary.compile_cloth_runtime_graph(graph)

after = {}
for old, new in mapping.items():
    asset = loaded[old]
    assert asset.get_path_name() == new + '.' + new.rsplit('/', 1)[1]
    after[old] = snapshot(asset)
    assert normalize(after[old]) == before[old], old
    if not verify:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)

assert all(hashlib.sha256(Path(f).read_bytes()).hexdigest() == sha for f, sha in plan['sources'].items())
result = dict(passed=True, packages=mapping, source_assets_unchanged=True,
              comparisons=len(after), default_deformers=4, installed=False)
(stage / ('reload.json' if verify else 'finalize.json')).write_text(json.dumps(result, indent=2))

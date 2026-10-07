"""Read UE 5.6.1 authoring mesh structure without saving assets."""
import hashlib
import json
import os
from pathlib import Path

import unreal


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def object_path(value):
    return value.get_path_name() if value is not None else None


stage = Path(os.environ['CSS_RIG_AUDIT_STAGE']).resolve(strict=True)
plan = json.loads((stage / 'input.json').read_text())
if not unreal.SystemLibrary.get_engine_version().startswith('5.6.1-'):
    raise RuntimeError('Expected UE 5.6.1')
if (stage / 'meshes.json').exists():
    raise FileExistsError(stage / 'meshes.json')
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False)
rows = []
for spec in plan['meshes']:
    path = Path(spec['source_file'])
    if sha(path) != spec['sha256']:
        raise ValueError('Input changed: ' + str(path))
    mesh = unreal.load_asset(spec['package'])
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise ValueError('Could not load mesh: ' + spec['package'])
    raw = unreal.CSSRigAuditLibrary.snapshot_mesh(mesh)
    if raw.startswith('ERROR:'):
        raise ValueError(spec['package'] + ': ' + raw)
    slots = mesh.get_editor_property('materials')
    rows.append(dict(
        **spec, audit=json.loads(raw),
        skeleton=object_path(mesh.get_editor_property('skeleton')),
        physics=object_path(mesh.get_editor_property('physics_asset')),
        postprocess=object_path(mesh.get_editor_property('post_process_anim_blueprint')),
        clothing=[object_path(c) for c in mesh.get_editor_property('mesh_clothing_assets')],
        materials=[dict(slot=str(s.material_slot_name), material=object_path(s.material_interface))
                   for s in slots],
        dependencies=sorted(str(d) for d in registry.get_dependencies(spec['package'], options))))
    (stage / 'partial.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('CSS_RIG_MESH_AUDITED', spec['package'], flush=True)
    del mesh, slots
    unreal.SystemLibrary.collect_garbage()
if any(sha(Path(s['source_file'])) != s['sha256'] for s in plan['meshes']):
    raise ValueError('Original source changed')
(stage / 'meshes.json').write_text(json.dumps(dict(
    scope='Editable source and editor-built render buffers; cooked/game comparison pending',
    source_files_unchanged=True, meshes=rows), indent=2) + '\n')

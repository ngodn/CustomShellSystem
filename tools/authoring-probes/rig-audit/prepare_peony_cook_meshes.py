"""Give the checked Peony meshes shipping paths without changing deformation data."""
import hashlib
import json
import os
from pathlib import Path

import unreal


stage = Path(os.environ['CSS_PEONY_COOK_PREP']).resolve(strict=True)
assert stage.name in {'peony-cookprep1', 'peony-cookprep2'}
assert Path(unreal.Paths.project_dir()).resolve() == stage / 'editor'
verify = os.environ.get('CSS_PEONY_VERIFY') == '1'
reports = stage / ('reload' if verify else 'author')
reports.mkdir()
protected = json.loads((stage / 'inputs.json').read_text())
audit = unreal.CSSRigAuditLibrary
slots = ['Peony_leg_panels_Lower_2_L', 'Peony_leg_panels_Lower_2_R']
base = '/Game/CSS/eins0fx/EveSB/'
mapping = {
    base + 'Peony/Meshes/SK_Peony_BasisStudy3': base + 'Peony/Meshes/SK_Peony',
    base + 'Peony/Meshes/SK_Peony_Face_BasisStudy3': base + 'Faces/Peony/SK_Facial1',
}


def decode(value):
    if value.startswith('ERROR:'):
        raise RuntimeError(value)
    return json.loads(value)


def state(mesh):
    structure = decode(audit.snapshot_mesh(mesh))
    source = decode(audit.snapshot_mesh_sections(mesh, [], True))
    assert source.pop('mesh') == mesh.get_path_name()
    morphs = [m['name'] for m in structure['morphs']]
    return dict(structure=structure, source=source,
                render=decode(audit.snapshot_render_sections(mesh, [], morphs, True)),
                cloth=decode(audit.snapshot_cloth_custom_data(mesh)),
                buffers=decode(audit.check_cloth_basis_buffers(mesh, slots)),
                graph=mesh.get_default_mesh_deformer().get_path_name(),
                collection=mesh.get_target_mesh_deformers().get_path_name())


rows = []
try:
    for source_path, target_path in mapping.items():
        source = unreal.load_asset(source_path)
        assert source
        before = state(source)
        if verify:
            target = unreal.load_asset(target_path)
        else:
            assert not unreal.EditorAssetLibrary.does_asset_exist(target_path)
            directory, name = target_path.rsplit('/', 1)
            target = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, directory, source)
        assert target and target.get_path_name().split('.')[0] == target_path
        after = state(target)
        cloth = decode(audit.compare_mesh_cloth(source, target))
        report = dict(source=source_path, target=target_path, before=before, after=after, cloth=cloth)
        (reports / (target_path.rsplit('/', 1)[1] + '.json')).write_text(json.dumps(report, indent=2))
        assert before == after, 'Mesh data changed while assigning the shipping path'
        assert cloth['equal'] and not any(a['same_object'] for a in cloth['assets'])
        if not verify:
            assert unreal.EditorAssetLibrary.save_loaded_asset(target, False)
        rows.append(dict(source=source_path, target=target_path,
                         morphs=len(after['structure']['morphs']), preserved=True))
finally:
    for name, checksum in protected.items():
        assert hashlib.sha256(Path(name).read_bytes()).hexdigest() == checksum, name

(reports / 'result.json').write_text(json.dumps(dict(meshes=rows, reload=verify,
    inputs_unchanged=True, scope='Canonical editor mesh copies only. Not a cook, fit acceptance or installation.'), indent=2) + '\n')

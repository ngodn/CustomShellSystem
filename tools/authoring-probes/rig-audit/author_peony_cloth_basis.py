"""Bind Peony's two cloth panels to the existing shared body-morph graph in UE 5.6.1."""
import hashlib
import json
import os
from pathlib import Path

import unreal


stage = Path(os.environ['CSS_PEONY_CLOTH_STAGE']).resolve(strict=True)
assert stage.name == 'peony-clothmorph1'
assert Path(unreal.Paths.project_dir()).resolve() == stage / 'editor'
verify = os.environ.get('CSS_PEONY_CLOTH_VERIFY') == '1'
reports = stage / ('reload' if verify else 'author')
reports.mkdir()
protected = json.loads((stage / 'inputs.json').read_text())
content = stage / 'editor/Content'
copied = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in content.rglob('*.uasset')}
library = unreal.CSSRigAuditLibrary


def decode(value):
    if value.startswith('ERROR:'):
        raise RuntimeError(value)
    return json.loads(value)


def state(mesh):
    structure = decode(library.snapshot_mesh(mesh))
    morphs = [row['name'] for row in structure['morphs']]
    source = decode(library.snapshot_mesh_sections(mesh, [], True))
    assert source.pop('mesh') == mesh.get_path_name()
    return dict(structure=structure,
                source=source,
                render=decode(library.snapshot_render_sections(mesh, [], morphs, True)),
                cloth=decode(library.snapshot_cloth_custom_data(mesh)))


records = []
try:
    base = '/Game/CSS/eins0fx/Shared/Deformers/'
    graph = unreal.load_asset(base + 'DG_ClothMorphBody')
    collection = unreal.load_asset(base + 'MDC_ClothMorphBody')
    assert graph and collection
    graph_before = decode(library.snapshot_deformer(graph))
    assert list(collection.get_editor_property('mesh_deformers')) == [graph]
    slots = {'Peony_leg_panels_Lower_2_L', 'Peony_leg_panels_Lower_2_R'}
    expected_attributes = {f'CSS_Cloth{shape}{axis}' for shape in
                           ('Glute', 'Hip', 'Thigh', 'Waist', 'Tone') for axis in 'XYZ'}
    for name, count in [('SK_Peony', 22), ('SK_Peony_Face', 137)]:
        package = '/Game/CSS/eins0fx/EveSB/Peony/Meshes/' + name
        source = unreal.load_asset(package)
        assert source
        before = state(source)
        assert len(before['structure']['morphs']) == count
        cloth_groups = [g for g in before['render']['groups'] if g['cloth_asset'] >= 0]
        assert len(cloth_groups) == 2 and {g['slot'] for g in cloth_groups} == slots
        assert library.check_cloth_basis_buffers(source, sorted(slots)) == 'ERROR: missing basis render buffer'
        target = package + '_BasisStudy3'
        mesh = (unreal.load_asset(target) if verify else
                library.clone_cloth_basis_mesh_study(source, target, True, collection, graph))
        assert mesh, name
        assert mesh.get_default_mesh_deformer() == graph
        assert mesh.get_target_mesh_deformers() == collection
        after = state(mesh)
        comparison = decode(library.compare_mesh_cloth(source, mesh))
        buffers = decode(library.check_cloth_basis_buffers(mesh, sorted(slots)))
        assert library.check_cloth_basis_buffers(mesh, ['UG_Skirt']) == 'ERROR: missing cloth section'
        assert library.check_cloth_basis_buffers(mesh, [sorted(slots)[0]] * 2) == 'ERROR: duplicate cloth slot'
        report = dict(mesh=mesh.get_path_name(), before=before, after=after, cloth=comparison, buffers=buffers)
        (reports / (name + '.json')).write_text(json.dumps(report, indent=2))
        assert before == after, 'Existing source/render/rig/morph/cloth data changed'
        assert comparison['equal'] and not any(a['same_object'] for a in comparison['assets'])
        lod, = mesh.get_editor_property('lod_info')
        attributes = lod.get_editor_property('vertex_attributes')
        assert {str(a.get_editor_property('name')) for a in attributes} == expected_attributes
        assert all(a.get_editor_property('enabled_for_render').get_editor_property('default') and
                   a.get_editor_property('data_type') == unreal.SkeletalMeshVertexAttributeDataType.FLOAT
                   for a in attributes)
        if not verify:
            assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        records.append(dict(mesh=mesh.get_path_name(), morphs=count, cloth_sections=2,
                            attributes=len(attributes), negative_checks=3, existing_data_equal=True))
    assert decode(library.snapshot_deformer(graph)) == graph_before
finally:
    for path, checksum in {**protected, **copied}.items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == checksum, path

(reports / 'result.json').write_text(json.dumps(dict(
    meshes=records, protected_inputs=len(protected), reload=verify,
    graph=graph.get_path_name(), graph_unchanged=True, installed=False,
    scope='Saved authoring data only. GPU rendering, cook and gameplay remain unverified.'
), indent=2) + '\n')

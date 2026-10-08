"""Author private glute-basis meshes and a graph without changing source assets."""
import hashlib
import json
import os
from pathlib import Path

import unreal


p = Path(os.environ['CSS_PANEL_STAGE']).resolve(strict=True)
all_shapes = os.environ.get('CSS_PANEL_ALL_SHAPES') == '1'
runtime_bindings = os.environ.get('CSS_PANEL_RUNTIME_BINDS') == '1'
normal_meshes = os.environ.get('CSS_PANEL_NORMAL_MESHES') == '1'
assert not runtime_bindings or all_shapes
assert not normal_meshes or runtime_bindings
out = p / ('basis-runtime-normal1' if normal_meshes else 'basis-runtime1' if runtime_bindings else
           'basis-study3' if all_shapes else 'basis-study2')
out.mkdir(exist_ok=True)
verify_only = os.environ.get('CSS_PANEL_VERIFY_BASIS') == '1'
reports = out / 'reload' if verify_only else out
reports.mkdir(exist_ok=True)
inputs = [Path(x) for x in json.loads((p / 'sources.json').read_text())]
protected = {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}
records = []


def snapshot(text):
    if text.startswith('ERROR:'):
        raise RuntimeError(text)
    return json.loads(text)


try:
    source = unreal.load_asset('/DeformerGraph/Deformers/DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals')
    graph_number = '5' if runtime_bindings else '4' if all_shapes else '3'
    graph_path = '/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy' + graph_number
    graph_exists = unreal.EditorAssetLibrary.does_asset_exist(graph_path)
    if graph_exists:
        graph = unreal.load_asset(graph_path)
        graph_receipt = p / 'basis-runtime1/graph.json' if normal_meshes else out / 'graph.json'
        previous = json.loads(graph_receipt.read_text())
        assert snapshot(unreal.CSSRigAuditLibrary.snapshot_deformer(graph)) == previous
    else:
        assert not verify_only, 'Missing saved graph'
        graph = unreal.CSSRigAuditLibrary.clone_cloth_basis_graph_study(source, graph_path, all_shapes, runtime_bindings)
    assert graph, 'Cannot author basis graph'
    if not verify_only:
        (out / 'graph.json').write_text(json.dumps(snapshot(unreal.CSSRigAuditLibrary.snapshot_deformer(graph)), indent=2))
        if not graph_exists:
            assert unreal.EditorAssetLibrary.save_loaded_asset(graph, False)
    collection = None
    if all_shapes:
        collection_name = 'MDC_ClothMorphStudy' + graph_number
        collection_path = '/Game/CSS/eins0fx/Shared/Deformers/' + collection_name
        if unreal.EditorAssetLibrary.does_asset_exist(collection_path):
            collection = unreal.load_asset(collection_path)
        else:
            assert not verify_only, 'Missing deformer collection'
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('data_asset_class', unreal.MeshDeformerCollection)
            collection = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                collection_name, '/Game/CSS/eins0fx/Shared/Deformers', unreal.MeshDeformerCollection, factory)
            assert collection
            collection.set_editor_property('mesh_deformers', [graph])
            assert unreal.EditorAssetLibrary.save_loaded_asset(collection, False)
        assert [x.get_path_name() for x in collection.get_editor_property('mesh_deformers')] == [graph.get_path_name()]
    for variant in ('Eve', 'Genessa'):
        package = (f'/Game/CSS/eins0fx/UnholyGenessa/SK_{variant}W3' if normal_meshes else
                   f'/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_{variant}W3_Face')
        source_mesh = unreal.load_asset(package)
        source_state = snapshot(unreal.CSSRigAuditLibrary.snapshot_mesh(source_mesh))
        morphs = [m['name'] for m in source_state['morphs']]
        before = snapshot(unreal.CSSRigAuditLibrary.snapshot_render_sections(source_mesh, [], morphs, True))
        target = package + ('_BasisStudy3' if runtime_bindings else '_BasisStudy2' if all_shapes else '_BasisStudy1')
        mesh = (unreal.load_asset(target) if verify_only else
                unreal.CSSRigAuditLibrary.clone_cloth_basis_mesh_study(
                    source_mesh, target, all_shapes, collection, graph if runtime_bindings else None))
        assert mesh, variant
        if runtime_bindings:
            assert mesh.get_default_mesh_deformer() == graph
        if collection:
            assert mesh.get_target_mesh_deformers() == collection
        assert snapshot(unreal.CSSRigAuditLibrary.snapshot_mesh(mesh)) == source_state, 'Changed rig or mesh structure'
        after = snapshot(unreal.CSSRigAuditLibrary.snapshot_render_sections(mesh, [], morphs, True))
        comparison = snapshot(unreal.CSSRigAuditLibrary.compare_mesh_cloth(source_mesh, mesh))
        result = dict(variant=variant, mesh=mesh.get_path_name(), render_equal=before == after,
                      cloth=comparison, before=before, after=after)
        (reports / f'{variant}.json').write_text(json.dumps(result, indent=2))
        assert result['render_equal'] and comparison['equal'], variant
        assert not any(a['same_object'] for a in comparison['assets']), 'Cloth data aliases original'
        if not verify_only:
            assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        records.append(dict(variant=variant, mesh=mesh.get_path_name(), render_equal=True, cloth_equal=True,
                            morphs_checked=len(morphs), sections_checked=len(before['groups'])))
finally:
    assert all(hashlib.sha256(Path(f).read_bytes()).hexdigest() == h for f, h in protected.items())

(reports / 'result.json').write_text(json.dumps(dict(
    graph=graph.get_path_name(), meshes=records, protected=protected,
    scope='Cloth-basis authoring and unchanged existing render/cloth data. GPU, cook and game checks pending.'
), indent=2))

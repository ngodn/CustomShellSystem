"""Author private glute-basis meshes and a graph without changing source assets."""
import hashlib
import json
import os
from pathlib import Path

import unreal


p = Path(os.environ['CSS_PANEL_STAGE']).resolve(strict=True)
out = p / 'basis-study2'
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
    graph_path = '/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy3'
    if unreal.EditorAssetLibrary.does_asset_exist(graph_path):
        graph = unreal.load_asset(graph_path)
        previous = json.loads((out / 'graph.json').read_text())
        assert snapshot(unreal.CSSRigAuditLibrary.snapshot_deformer(graph)) == previous
    else:
        assert not verify_only, 'Missing saved graph'
        graph = unreal.CSSRigAuditLibrary.clone_cloth_basis_graph_study(source, graph_path)
    assert graph, 'Cannot author basis graph'
    if not verify_only:
        (out / 'graph.json').write_text(json.dumps(snapshot(unreal.CSSRigAuditLibrary.snapshot_deformer(graph)), indent=2))
        assert unreal.EditorAssetLibrary.save_loaded_asset(graph, False)
    for variant in ('Eve', 'Genessa'):
        package = f'/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_{variant}W3_Face'
        source_mesh = unreal.load_asset(package)
        source_state = snapshot(unreal.CSSRigAuditLibrary.snapshot_mesh(source_mesh))
        morphs = [m['name'] for m in source_state['morphs']]
        before = snapshot(unreal.CSSRigAuditLibrary.snapshot_render_sections(source_mesh, [], morphs, True))
        mesh = (unreal.load_asset(package + '_BasisStudy1') if verify_only else
                unreal.CSSRigAuditLibrary.clone_cloth_basis_mesh_study(source_mesh, package + '_BasisStudy1'))
        assert mesh, variant
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
    scope='Glute-basis authoring and unchanged existing render/cloth data. GPU, cook and game checks pending.'
), indent=2))

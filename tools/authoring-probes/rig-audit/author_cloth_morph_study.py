"""Run in a private UE 5.6.1 commandlet with CSSRigAudit and DeformerGraph enabled."""
import hashlib
import json
import os
from pathlib import Path

import unreal


output = Path(os.environ['CSS_PANEL_STAGE']).resolve(strict=True)
engine_content = Path(os.environ['CSS_DEFORMER_CONTENT']).resolve(strict=True)
protected = sorted(engine_content.rglob('*.uasset'))
if not protected:
    raise RuntimeError('No engine deformer assets found')


def hashes():
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}


before = hashes()
try:
    source = unreal.load_asset('/DeformerGraph/Deformers/DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals')
    asset = unreal.CSSRigAuditLibrary.clone_cloth_morph_study(
        source, '/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy1')
    if not asset:
        raise RuntimeError('Private graph authoring failed; see Cloth morph study diagnostics')
    snapshot = json.loads(unreal.CSSRigAuditLibrary.snapshot_deformer(asset))
    (output / 'candidate-graph.json').write_text(json.dumps(snapshot, indent=2) + '\n')
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError('Cannot save the private graph')
finally:
    if hashes() != before:
        raise RuntimeError('Protected engine assets changed')

(output / 'result.json').write_text(json.dumps(dict(
    asset=asset.get_path_name(), engine_assets_unchanged=True, protected_sha256=before,
    scope='Graph compile/save only; GPU shaders, execution, fit and collision unverified.'
), indent=2) + '\n')

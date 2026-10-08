"""Compare full and streaming mesh audits in an isolated UE commandlet."""
import json
import os
from pathlib import Path

import unreal

stage = Path(os.environ['CSS_NAMESPACE_STAGE']).resolve(strict=True)
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve() == stage / 'Content'
output = stage / 'array-digest-probe'
output.mkdir(exist_ok=False)
mesh = unreal.load_asset('/Game/CSS/CommanderWhite/Dev/SK_CommanderC4')
assert isinstance(mesh, unreal.SkeletalMesh)
audit = unreal.CSSRigAuditLibrary
summary = json.loads(audit.snapshot_mesh(mesh))
materials = mesh.get_editor_property('materials')
sections = summary['render_lods'][0]['sections']
selected = [min(sections, key=lambda s: s['vertices'])]
cloth = [s for s in sections if s['cloth_asset_index'] >= 0]
if cloth:
    selected.append(min(cloth, key=lambda s: s['vertices']))
slots = list(dict.fromkeys(str(materials[s['material_index']].get_editor_property('material_slot_name')) for s in selected))
morphs = [m['name'] for m in summary['morphs']]
for kind, call in (
        ('render', lambda slots, digest: audit.snapshot_render_sections(mesh, slots, morphs, digest)),
        ('source', lambda slots, digest: audit.snapshot_mesh_sections(mesh, slots, digest))):
    for mode in ('full', 'digest'):
        raw = call(slots, mode == 'digest')
        assert not raw.startswith('ERROR:'), raw
        (output / (kind + '-' + mode + '.json')).write_text(raw, encoding='utf-8')
    for bad in (['CSS_NoSuchSlot_Probe'], [slots[0], slots[0]]):
        assert call(bad, True).startswith('ERROR:')
    raw = call([], True)
    assert not raw.startswith('ERROR:'), raw
    (output / (kind + '-all-digest.json')).write_text(raw, encoding='utf-8')
    print('CSS_ARRAY_DIGEST_PROBE', kind, 'all sections checked', flush=True)
(output / 'result.json').write_text(json.dumps(dict(slots=slots, summary=summary,
    full_mesh_completed=True, invalid_requests_rejected=True), indent=2))

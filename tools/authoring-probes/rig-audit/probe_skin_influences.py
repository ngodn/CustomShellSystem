"""Record section-level skin influences without exporting large render arrays."""
import hashlib
import json
import os
from pathlib import Path

import unreal

output = Path(os.environ['CSS_SKIN_INFLUENCES_OUTPUT']).resolve(strict=True)
manifest_path = Path(os.environ['CSS_SKIN_INFLUENCES_MANIFEST']).resolve(strict=True)
manifest = json.loads(manifest_path.read_text())
content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))
rows = []
for entry in manifest:
    package = entry['package']
    if not package.startswith('/Game/'):
        raise ValueError('Expected a project asset')
    source = (content / (package.removeprefix('/Game/') + '.uasset')).resolve(strict=True)
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    mesh = unreal.load_asset(package)
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise ValueError('Missing mesh: ' + package)
    raw = unreal.CSSRigAuditLibrary.snapshot_mesh(mesh, True)
    if raw.startswith('ERROR:'):
        raise RuntimeError(raw)
    report = json.loads(raw)
    for lod in report['render_lods']:
        for section in lod['sections']:
            if any(section[key] for key in ('invalid_weight_bones', 'unweighted_vertices', 'nonfinite_positions')):
                raise ValueError('Invalid skin data: ' + package)
            if section['vertices'] and (section['min_weight_sum_u16'] != 65535 or section['max_weight_sum_u16'] != 65535):
                raise ValueError('Unnormalized weights: ' + package)
            if sum(b['weight_sum_u16'] for b in section['weighted_bones'].values()) != section['vertices'] * 65535:
                raise ValueError('Incomplete influence coverage: ' + package)
    # The optional summary must not alter the existing audit or its caller contract.
    basic = json.loads(unreal.CSSRigAuditLibrary.snapshot_mesh(mesh))
    stripped = json.loads(raw)
    for lod in stripped['render_lods']:
        for section in lod['sections']:
            del section['material_slot'], section['weighted_bones']
    if basic != stripped:
        raise ValueError('Coverage changed the base snapshot')
    if hashlib.sha256(source.read_bytes()).hexdigest() != before:
        raise ValueError('Source asset changed')
    target = output / (str(len(rows)) + '.json')
    target.write_text(raw + '\n')
    rows.append(dict(package=package, source=str(source), source_sha256=before,
                     report=str(target), report_sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
    unreal.log('CSS_SKIN_INFLUENCES ' + package)
(output / 'report.json').write_text(json.dumps(dict(passed=True, meshes=rows,
    manifest_sha256=hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
    scope='Editor render weights by material section. Bone influence counts are not unique affected-vertex counts when combined; ancestor propagation and cloth transfer are separate.'), indent=2) + '\n')

"""Read authored foot shapes and their drivers without saving the source."""
import hashlib
import json
from pathlib import Path

import bpy
import numpy as np

root = Path(__file__).resolve().parents[3]
source = root / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/reference/body-type-variant-EVE/eve_beta10.blend'
output = root / 'CustomShellSystem/work/eve26/author-foot-controls2.json'
assert not output.exists()
digest = hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(source), link=False) as (_, destination):
    destination.objects = ['Eve Body', 'Eve Extras - Heels']

rows = []
for obj in destination.objects:
    assert obj is not None
    keys = obj.data.shape_keys
    shapes = []
    if keys:
        basis = np.array([v.co[:] for v in keys.reference_key.data])
        for key in keys.key_blocks:
            points = np.array([v.co[:] for v in key.data])
            displacement = np.linalg.norm(points - basis, axis=1)
            changed = displacement > 1e-7
            relative = np.array([v.co[:] for v in key.relative_key.data])
            mask = np.ones(len(points))
            if key.vertex_group:
                group = obj.vertex_groups[key.vertex_group].index
                mask = np.array([next((g.weight for g in vertex.groups if g.group == group), 0.0)
                    for vertex in obj.data.vertices])
            effective = (points - relative) * mask[:, None]
            effective_length = np.linalg.norm(effective, axis=1)
            effective_changed = effective_length > 1e-7
            shapes.append(dict(name=key.name, value=key.value,
                relative_key=key.relative_key.name,
                vertex_group=key.vertex_group, changed_vertices=int(changed.sum()),
                masked_changed_vertices=int(effective_changed.sum()),
                masked_maximum_delta_cm=float(effective_length.max() * 100),
                masked_affected_local_z_cm=[float(basis[effective_changed, 2].min() * 100),
                    float(basis[effective_changed, 2].max() * 100)] if effective_changed.any() else None,
                maximum_delta_cm=float(displacement.max() * 100),
                affected_local_z_cm=[float(basis[changed, 2].min() * 100),
                    float(basis[changed, 2].max() * 100)] if changed.any() else None))
    drivers = []
    if keys and keys.animation_data:
        for curve in keys.animation_data.drivers:
            drivers.append(dict(path=curve.data_path, expression=curve.driver.expression,
                variables=[dict(name=v.name, type=v.type,
                    targets=[dict(id=t.id.name if t.id else None, path=t.data_path,
                        bone=t.bone_target) for t in v.targets]) for v in curve.driver.variables]))
    rows.append(dict(object=obj.name, shapes=shapes, drivers=drivers))

assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
output.write_text(json.dumps(dict(source_sha256=digest, source_unchanged=True,
    objects=rows), indent=2) + '\n')
print(str(output), flush=True)

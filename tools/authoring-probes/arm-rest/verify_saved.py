"""Independently reopen both blends and verify each coordinate and morph."""
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix

import os
OUT = Path(os.environ['CSS_ARM_REST_AUDIT_DIR']).resolve()
if OUT.parent != Path(__file__).resolve().parents[3] / 'work/grip-grounding-v1':
    raise ValueError('Audit directory must be a direct workspace grip-grounding child')
ROOT = OUT.parents[3]
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(MOD/'tools'))
from export_seduxtress_eve import TO_UE
from separate_nextgen_footwear import coordinates, face_records

report = json.loads((OUT/'candidate.json').read_text())
transforms = {r['name']: Matrix(r['matrix']) for r in report['geometry_transforms_ue']}
bpy.ops.wm.open_mainfile(filepath=str(MOD/'work/CSS_SeduXtress_HandBindV43.blend'))
original = {}
for name in report['parts']:
    obj = bpy.data.objects[name]
    rig = next(m.object for m in obj.modifiers if m.type == 'ARMATURE')
    basis = TO_UE @ obj.matrix_world
    matrices = {name: np.asarray(basis.inverted() @ m @ basis, dtype=np.float64) for name, m in transforms.items()}
    deform = {b.name for b in rig.data.bones if b.use_deform}
    skin = np.repeat(np.eye(4)[None, :, :], len(obj.data.vertices), axis=0)
    weights = [{obj.vertex_groups[g.group].name: g.weight for g in v.groups} for v in obj.data.vertices]
    for i, row in enumerate(weights):
        names = [n for n, w in row.items() if n in deform and w > 0]
        if names:
            w = np.asarray([row[n] for n in names], dtype=np.float64)
            w /= w.sum()
            skin[i] = np.tensordot(w, np.stack([matrices[n] for n in names]), axes=1)
    arrays = {'vertices': coordinates(obj.data.vertices)}
    meta = {}
    for key in obj.data.shape_keys.key_blocks if obj.data.shape_keys else []:
        arrays[key.name] = coordinates(key.data)
        meta[key.name] = dict(relative=key.relative_key.name, value=key.value, mute=key.mute,
                              vertex_group=key.vertex_group, slider_min=key.slider_min, slider_max=key.slider_max)
    original[name] = dict(arrays=arrays, keys=meta, weights=weights, skin=skin,
                          faces=face_records(obj), materials=[m.name if m else None for m in obj.data.materials],
                          matrix=[list(r) for r in obj.matrix_world])

bpy.ops.wm.open_mainfile(filepath=report['output'])
results = {}
for name, source in original.items():
    obj = bpy.data.objects[name]
    assert [list(r) for r in obj.matrix_world] == source['matrix']
    assert face_records(obj) == source['faces']
    assert [m.name if m else None for m in obj.data.materials] == source['materials']
    assert [{obj.vertex_groups[g.group].name: g.weight for g in v.groups} for v in obj.data.vertices] == source['weights']
    arrays = {'vertices': coordinates(obj.data.vertices)}
    meta = {}
    for key in obj.data.shape_keys.key_blocks if obj.data.shape_keys else []:
        arrays[key.name] = coordinates(key.data)
        meta[key.name] = dict(relative=key.relative_key.name, value=key.value, mute=key.mute,
                              vertex_group=key.vertex_group, slider_min=key.slider_min, slider_max=key.slider_max)
    assert meta == source['keys'], name
    assert arrays.keys() == source['arrays'].keys(), name
    inverse = np.linalg.inv(source['skin'])
    errors = {}
    for key, actual in arrays.items():
        homogeneous = np.column_stack((actual, np.ones(len(actual))))
        recovered = np.einsum('nij,nj->ni', inverse, homogeneous)[:, :3]
        error = float(np.linalg.norm(recovered-source['arrays'][key], axis=1).max()*100)
        assert error < .001, (name, key, error)
        errors[key] = error
    results[name] = dict(shape_keys=len(meta), maximum_inverse_error_cm=max(errors.values()), errors=errors)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in report['input_hashes'].items())
(OUT/'saved-validation.json').write_text(json.dumps(dict(parts=results, source_files_unchanged=True,
    morph_names_values_relative_keys_and_ranges_unchanged=True, weights_uv_topology_materials_unchanged=True,
    scope='Every stored vertex and morph coordinate independently inverse-mapped after reopening the saved candidate.'), indent=2)+'\n')
print(json.dumps(dict(parts=len(results), shape_keys=sum(x['shape_keys'] for x in results.values()),
    maximum_inverse_error_cm=max(x['maximum_inverse_error_cm'] for x in results.values()))), flush=True)

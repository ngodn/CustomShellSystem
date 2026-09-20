"""Extract local joint corrective drivers and compare with retained skin fixtures.

Blender 5.2.2. Diagnostic parameters only; no asset or source blend is saved.
"""
import ast
import hashlib
import json
import math
import os
import re
import sys
from pathlib import Path

import bpy
from mathutils import Quaternion

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_CORRECTIVE_DRIVER_AUDIT_DIR']).resolve()
assert OUT.parent == WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT / 'corrective-driver.json').exists()
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
AUDIT = MOD / 'work/nextgen-audit'
sys.path.insert(0, str(MOD / 'tools'))
from export_seduxtress_eve import TO_UE, read_bones

def load(path):
    return json.loads(path.read_text())

mapping = next(ast.literal_eval(n.value) for n in ast.parse((MOD / 'tools/bind_eve_to_css_base_clean.py').read_text()).body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'vg_mapping' for t in n.targets))
for finger in ('index', 'middle', 'ring', 'pinky'):
    mapping[f'palm_{finger}.L'] = f'{finger}_metacarpal_l'
bones, old_bind = read_bones(MOD / 'work/CSS_SeduXtress_HandBindV43.bindpose.json')
indices = {b['name']: i for i, b in enumerate(bones)}
source = MOD / 'reference/body-type-variant-EVE/eve_beta10.blend'
source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
rig = bpy.data.objects['Eve Armature']
basis = TO_UE @ rig.matrix_world
drivers = load(AUDIT / 'audit-hand-shape-drivers-v3.json')['source']['drivers']
models = []
for driver in drivers:
    if driver['owner'] != 'Genesis 8.1 Female' or not driver['path'].endswith('_L(fin)"]') or not any(s in driver['path'] for s in ('pJCMIndex', 'pJCMMid', 'pJCMRing', 'pJCMPinky', 'pJCMThumb')):
        continue
    target = driver['variables'][0]['targets'][0]
    assert target['space'] == 'LOCAL_SPACE'
    bone = rig.data.bones[target['bone']]
    curve = rig.data.animation_data.drivers.find(driver['path'])
    order = curve.driver.variables[0].targets[0].rotation_mode
    if order == 'AUTO':
        order = rig.pose.bones[bone.name].rotation_mode
    assert order in ('XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX'), order
    match = re.fullmatch(r'clamp\((-?[0-9.]+)\*A,0,1\)', driver['expression'])
    assert match
    child, parent = indices[mapping[bone.name]], indices[mapping[bone.parent.name]]
    assert bones[child]['parent'] == parent, 'Local driver requires the same direct parent'
    rest_inverse = (bone.parent.matrix_local.inverted() @ bone.matrix_local).inverted().to_quaternion()
    source_parent = (basis @ bone.parent.matrix_local).to_quaternion()
    source_child = (basis @ bone.matrix_local).to_quaternion()
    # Cancel the common animated parent transform from the old full-world
    # formulation. These constants consume the calibration's target locals,
    # not uncorrected game rotations or B2's neutral local rotations.
    left = (rest_inverse @ source_parent.inverted() @ old_bind[parent].to_quaternion()).normalized()
    right = (old_bind[child].to_quaternion().inverted() @ source_child).normalized()
    models.append(dict(shape=driver['path'][2:-7], bone=bones[child]['name'], index=child,
                       parent=bones[parent]['name'], coefficient=float(match[1]),
                       euler_order=order, axis=target['transform'][-1],
                       left_wxyz=list(left), right_wxyz=list(right)))
assert len(models) == 16

bpy.ops.wm.open_mainfile(filepath=str(MOD / 'work/CSS_SeduXtress_ArmRestV44B2.blend'), use_scripts=False)
keys = bpy.data.objects['Eve Body'].data.shape_keys.key_blocks
for model in models:
    model['baked_value'] = keys[model['shape']].value

def evaluate(doc):
    snapshot = doc['pose']['Snapshot']
    values = {}
    for model in models:
        i = model['index']
        assert snapshot['BoneNames'][i].lower() == model['bone']
        transform = snapshot['LocalTransforms'][i]
        assert all(abs(transform['Scale3D'][axis] - 1) < 1e-5 for axis in 'XYZ')
        q = Quaternion([transform['Rotation'][axis] for axis in 'WXYZ']).normalized()
        original = (Quaternion(model['left_wxyz']) @ q @ Quaternion(model['right_wxyz'])).normalized()
        radians = original.to_euler(model['euler_order'])['XYZ'.index(model['axis'])]
        value = max(0.0, min(1.0, model['coefficient'] * radians))
        assert math.isfinite(value)
        values[model['shape']] = value
    return values

checks = []
fixtures = []
controls = AUDIT / 'left-corrected-controller-sweep-v1'
for degrees in range(0, 61, 5):
    fixtures.append((f'control-{degrees}', controls / f'control-{degrees}.json', controls / f'shapes-{degrees}.json'))
for case in load(AUDIT / 'left-observed-thumb-clearance-v1/manifest.json')['cases']:
    name = case['pose']
    fixtures.append((name, AUDIT / 'left-observed-thumb-clearance-v1' / name,
                     AUDIT / 'left-observed-thumb-clearance-v1-contacts' / ('shapes-' + name)))
for label, pose, shape in fixtures:
    values = evaluate(load(pose))
    expected = load(shape)['values']
    assert values.keys() == expected.keys()
    errors = {name: abs(value - expected[name]) for name, value in values.items()}
    checks.append(dict(fixture=label, max_weight_error=max(errors.values()), errors=errors))
    assert max(errors.values()) < 1e-5, (label, errors)

assert hashlib.sha256(source.read_bytes()).hexdigest() == source_hash
(OUT / 'corrective-driver.json').write_text(json.dumps(dict(schema=1,
    scope='Diagnostic local driver. Consumes calibrated target-local rotations; not raw game or neutral B2 rotations.',
    source_blend_sha256=source_hash, quaternion_order='WXYZ', angle_units='radians',
    output='desired_source_value minus baked_value', parameters=models), indent=2) + '\n')
(OUT / 'validation.json').write_text(json.dumps(dict(fixtures=len(checks), values=len(checks) * len(models),
    max_weight_error=max(r['max_weight_error'] for r in checks), checks=checks,
    scope='Matches retained full-world source-rig corrective evaluation. Does not validate UE Euler conventions or runtime graph execution.'), indent=2) + '\n')
print(json.dumps(dict(drivers=len(models), orders=sorted({m['euler_order'] for m in models}), fixtures=len(checks),
                      max_weight_error=max(r['max_weight_error'] for r in checks))), flush=True)

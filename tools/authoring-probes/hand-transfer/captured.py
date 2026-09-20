"""Replay retained captured-hand corrections on V44B2 with actual Blender skinning.

Only hand deformation transfer is tested. Captured body rotations still belong
to V43 and are not a claim about V44's complete animation graph.
"""
import hashlib
import json
import os
import sys
import time
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_CAPTURED_HAND_AUDIT_DIR']).resolve()
assert OUT.parent == WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT / 'report.json').exists()
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
AUDIT = MOD / 'work/nextgen-audit'
sys.path.insert(0, str(MOD / 'tools'))
from export_seduxtress_eve import read_bones, TO_UE, EXPORT_SHAPES
from hand_skin_intersections import audit

def load(path):
    return json.loads(path.read_text())

def pair_set(result):
    return {tuple(sorted(tuple(sorted(t)) for t in p['triangles'])) for p in result['pairs']}

blend = MOD / 'work/CSS_SeduXtress_ArmRestV44B2.blend'
source_hash = hashlib.sha256(blend.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(blend), use_scripts=False)
body = bpy.data.objects['Eve Body']
rig = bpy.data.objects['SKEL_CSS_Base']
bones, bind = read_bones(MOD / rig['CSS_bind_pose'])
index = {b['name'].lower(): i for i, b in enumerate(bones)}
assert len(bones) == 379
finger_indices = {i for n, i in index.items() if n.endswith('_l') and n.startswith(('thumb_', 'index_', 'middle_', 'ring_', 'pinky_'))}
assert len(finger_indices) == 19
for obj in (body, rig):
    obj.hide_viewport = False
    obj.hide_set(False)
rig.animation_data_clear()
for pb in rig.pose.bones:
    assert not pb.constraints
    pb.matrix_basis = Matrix.Identity(4)
for modifier in list(body.modifiers):
    if modifier.type != 'ARMATURE':
        body.modifiers.remove(modifier)
    else:
        modifier.use_deform_preserve_volume = False
keys = body.data.shape_keys.key_blocks
body.data.shape_keys.animation_data_clear()
defaults = {key.name: key.value for key in keys}
basis = TO_UE @ rig.matrix_world

def evaluate(doc, shapes):
    snapshot = doc['pose']['Snapshot']
    assert snapshot['bIsValid']
    assert [n.lower() for n in snapshot['BoneNames'][:379]] == list(index)
    world = []
    for bone, transform in zip(bones, snapshot['LocalTransforms'][:379], strict=True):
        q = Quaternion([transform['Rotation'][k] for k in 'WXYZ']).normalized()
        local = Matrix.LocRotScale(Vector(bone['translation']), q, Vector([transform['Scale3D'][k] for k in 'XYZ']))
        world.append(world[bone['parent']] @ local if bone['parent'] >= 0 else local)
    for bone, rest, posed in zip(bones, bind, world, strict=True):
        pb = rig.pose.bones[bone['name']]
        pb.matrix = basis.inverted() @ posed @ rest.inverted() @ basis @ pb.bone.matrix_local
        bpy.context.view_layer.update()
    for key in keys:
        key.value = 0 if key.name in EXPORT_SHAPES else shapes.get(key.name, defaults[key.name])
    bpy.context.view_layer.update()
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    result = audit(body, mesh, 'l')
    evaluated.to_mesh_clear()
    return result

controls = AUDIT / 'left-corrected-controller-sweep-v1'
neutral = evaluate(load(controls / 'control-0.json'), load(controls / 'shapes-0.json')['values'])
neutral_pairs = pair_set(neutral)
assert len(neutral_pairs) == 4
(OUT / 'neutral-contacts.json').write_text(json.dumps(neutral, indent=2) + '\n')

cases = load(AUDIT / 'left-observed-thumb-clearance-v1/manifest.json')['cases']
assert len(cases) == 35
rows = []
started = time.monotonic()
for case in cases:
    name = case['pose']
    path = AUDIT / 'left-observed-thumb-clearance-v1' / name
    shapes_path = AUDIT / 'left-observed-thumb-clearance-v1-contacts' / ('shapes-' + name)
    doc = load(path)
    original_path = Path(case['source'])
    assert original_path.resolve().is_relative_to(WORK)
    original = load(original_path)
    before, after = [d['pose']['Snapshot']['LocalTransforms'] for d in (original, doc)]
    assert all(a == b for i, (a, b) in enumerate(zip(before, after, strict=True)) if i not in finger_indices)
    assert all(a[k] == b[k] for a, b in zip(before, after, strict=True) for k in ('Translation', 'Scale3D'))
    shapes = load(shapes_path)['values']
    result = evaluate(doc, shapes)
    extra = pair_set(result) - neutral_pairs
    row = dict(case, total_pairs=result['crossing_pairs'], regions=result['regions'], new_pairs=len(extra),
               correction_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
               shape_sha256=hashlib.sha256(shapes_path.read_bytes()).hexdigest())
    (OUT / ('contacts-' + name)).write_text(json.dumps(result, indent=2) + '\n')
    rows.append(row)
    print(json.dumps(dict(pose=name, new_pairs=len(extra), total_pairs=result['crossing_pairs'])), flush=True)

# Preserve the old failing input as a red-capable check on the new geometry.
name = 'hand-overlay-observation-v2-pose-11.json'
path = AUDIT / 'left-observed-graph-directional-v1' / name
shape_path = AUDIT / 'left-observed-graph-directional-v1-contacts' / ('shapes-' + name)
residual = evaluate(load(path), load(shape_path)['values'])
(OUT / 'before-thumb-clearance-contacts.json').write_text(json.dumps(residual, indent=2) + '\n')
assert hashlib.sha256(blend.read_bytes()).hexdigest() == source_hash
report = dict(scope=__doc__, blend=str(blend), blend_sha256=source_hash, source_blend_unchanged=True,
              samples=rows, before_thumb_clearance_new_pairs=len(pair_set(residual) - neutral_pairs),
              passes=sum(r['new_pairs'] == 0 for r in rows), seconds=time.monotonic() - started)
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(dict(passes=report['passes'], total=len(rows), before_thumb_clearance_new_pairs=report['before_thumb_clearance_new_pairs'])), flush=True)
raise SystemExit(0 if report['passes'] == len(rows) and report['before_thumb_clearance_new_pairs'] > 0 else 2)

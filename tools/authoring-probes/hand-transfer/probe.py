"""Blender 5.2.2: check whether V43 finger calibration survives the V44B2 bake.

Evidence only. Controls are source-Eve poses, not replacement game animations.
"""
import copy
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

import os

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_HAND_TRANSFER_AUDIT_DIR']).resolve()
assert OUT.parent == WORK.resolve(), 'Use a direct child of the grip evidence directory'
OUT.mkdir(exist_ok=True)
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
AUDIT = MOD / 'work/nextgen-audit'
sys.path.insert(0, str(MOD / 'tools'))
from export_seduxtress_eve import read_bones, TO_UE, EXPORT_SHAPES
from hand_skin_intersections import audit

assert not (OUT / 'report.json').exists()
models = json.loads((AUDIT / 'left-graph-anchor-animation-stress-v2/calibration-model.json').read_text())['parameters']
control_dir = AUDIT / 'left-corrected-controller-sweep-v1'
controls = {n: json.loads((control_dir / f'control-{n}.json').read_text()) for n in (0, 60)}
shapes = {n: json.loads((control_dir / f'shapes-{n}.json').read_text())['values'] for n in controls}
input_path = WORK / 'arm-rest-alignment-v1/candidate_game_rotations-s1-compressed-ik1.json'
game = json.loads(input_path.read_text())

def quat(t):
    return Quaternion([t['Rotation'][k] for k in 'WXYZ']).normalized()

def forward(raw, model):
    delta = (Quaternion(model['source_ref']).inverted() @ raw).normalized()
    if delta.w < 0:
        delta.negate()
    if model['axis'] is not None:
        axis = Vector(model['axis'])
        projection = Vector((delta.x, delta.y, delta.z)).dot(axis)
        norm = math.hypot(delta.w, projection)
        assert norm > 1e-5
        twist = Quaternion((delta.w / norm, *(axis * (projection / norm))))
        delta = delta @ twist.inverted() @ Quaternion(axis, 2 * math.atan2(projection, delta.w) * model['ratio'])
    transport = Quaternion(model['transport'])
    return (Quaternion(model['target_ref']) @ transport @ delta @ transport.inverted()).normalized()

def pairs(result):
    return {tuple(sorted(tuple(sorted(t)) for t in p['triangles'])) for p in result['pairs']}

results = []
reference_points = {}
binds = {}
for version, filename in [('v43', 'CSS_SeduXtress_HandBindV43'), ('v44b2', 'CSS_SeduXtress_ArmRestV44B2')]:
    blend = MOD / f'work/{filename}.blend'
    bpy.ops.wm.open_mainfile(filepath=str(blend), use_scripts=False)
    body = bpy.data.objects['Eve Body']
    rig = bpy.data.objects['SKEL_CSS_Base']
    bones, bind = read_bones(MOD / rig['CSS_bind_pose'])
    binds[version] = bones
    index = {b['name'].lower(): i for i, b in enumerate(bones)}
    assert len(bones) == 379
    rig.animation_data_clear()
    rig.hide_viewport = False
    rig.hide_set(False)
    body.hide_viewport = False
    body.hide_set(False)
    for modifier in list(body.modifiers):
        if modifier.type != 'ARMATURE':
            body.modifiers.remove(modifier)
        else:
            modifier.use_deform_preserve_volume = False
    keys = body.data.shape_keys.key_blocks
    body.data.shape_keys.animation_data_clear()
    defaults = {key.name: key.value for key in keys}
    basis = TO_UE @ rig.matrix_world
    hand_ids = [v.index for v in body.data.vertices if sum(g.weight for g in v.groups if body.vertex_groups[g.group].name.endswith('_l') and body.vertex_groups[g.group].name.startswith(('hand_', 'thumb_', 'index_', 'middle_', 'ring_', 'pinky_'))) > .999]
    cases = [(f'control-{n}', controls[n], shapes[n]) for n in controls]
    if version == 'v44b2':
        tracks = json.loads((WORK / 'active-h2-absolute-tracks-v1/absolute-tracks.json').read_text())
        input_checks = []
        for sample in range(5):
            game = json.loads((WORK / f'arm-rest-alignment-v1/candidate_game_rotations-s{sample}-compressed-ik1.json').read_text())
            engine_raw = json.loads((WORK / f'arm-rest-alignment-v1/candidate_game_rotations-s{sample}-raw-ik1.json').read_text())
            raw = tracks['frames'][sample]
            names = {n.lower(): i for i, n in enumerate(raw['BoneNames'])}
            for model in models:
                incoming = quat(engine_raw['pose']['Snapshot']['LocalTransforms'][model['index']])
                original = quat(raw['LocalTransforms'][names[model['bone']]])
                d = incoming.rotation_difference(original).normalized()
                angle = math.degrees(2 * math.atan2(Vector((d.x,d.y,d.z)).length, abs(d.w)))
                assert angle < .001, (sample, model['bone'], angle)
                compressed = quat(game['pose']['Snapshot']['LocalTransforms'][model['index']])
                dq = compressed.rotation_difference(incoming).normalized()
                compression_angle = math.degrees(2 * math.atan2(Vector((dq.x,dq.y,dq.z)).length, abs(dq.w)))
                input_checks.append(dict(sample=sample, bone=model['bone'], raw_rotation_error_degrees=angle, compression_difference_degrees=compression_angle))
            cases.append((f'h2-s{sample}-baseline', game, {}))
            calibrated = copy.deepcopy(game)
            for model in models:
                i = index[model['bone']]
                assert i == model['index']
                q = forward(quat(game['pose']['Snapshot']['LocalTransforms'][i]), model)
                calibrated['pose']['Snapshot']['LocalTransforms'][i]['Rotation'] = dict(zip('XYZW', (q.x, q.y, q.z, q.w)))
            # Input uses the game-rotation reference. Applying V43's inverse remap
            # here would undo a conversion which is absent from this fixture.
            changed = {index[m['bone']] for m in models}
            before = game['pose']['Snapshot']['LocalTransforms']
            after = calibrated['pose']['Snapshot']['LocalTransforms']
            assert all(a == b for i, (a, b) in enumerate(zip(before, after, strict=True)) if i not in changed)
            assert all(a[k] == b[k] for a, b in zip(before, after, strict=True) for k in ('Translation', 'Scale3D'))
            (OUT / f'h2-s{sample}-calibrated.json').write_text(json.dumps(calibrated, indent=2) + '\n')
            cases.append((f'h2-s{sample}-calibrated-no-correctives', calibrated, {}))
        (OUT / 'input-domain-checks.json').write_text(json.dumps(input_checks, indent=2) + '\n')
    neutral = None
    for label, doc, values in cases:
        snapshot = doc['pose']['Snapshot']
        assert [n.lower() for n in snapshot['BoneNames'][:379]] == list(index)
        world = []
        for i, (bone, t) in enumerate(zip(bones, snapshot['LocalTransforms'][:379], strict=True)):
            # Same local rotation, target translations. Control fixtures use
            # the V43 body; the H2 candidate already has the aligned body.
            local = Matrix.LocRotScale(Vector(bone['translation']), quat(t), Vector([t['Scale3D'][k] for k in 'XYZ']))
            world.append(world[bone['parent']] @ local if bone['parent'] >= 0 else local)
        for pb in rig.pose.bones:
            assert not pb.constraints
            pb.matrix_basis = Matrix.Identity(4)
        for b, rest, posed in zip(bones, bind, world, strict=True):
            pb = rig.pose.bones[b['name']]
            pb.matrix = basis.inverted() @ posed @ rest.inverted() @ basis @ pb.bone.matrix_local
            bpy.context.view_layer.update()
        for key in keys:
            key.value = 0 if key.name in EXPORT_SHAPES else values.get(key.name, defaults[key.name])
        bpy.context.view_layer.update()
        evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
        mesh = evaluated.to_mesh()
        contact = audit(body, mesh, 'l')
        local_to_hand = world[index['hand_l']].inverted() @ TO_UE @ body.matrix_world
        points = np.asarray([list(local_to_hand @ mesh.vertices[i].co) for i in hand_ids])
        evaluated.to_mesh_clear()
        if label == 'control-0':
            neutral = pairs(contact)
        row = dict(version=version, case=label, crossing_pairs=contact['crossing_pairs'], regions=contact['regions'], new_pairs=len(pairs(contact) - neutral), hand_vertices=len(hand_ids))
        if version == 'v43':
            reference_points[label] = points
        elif label in reference_points:
            error = np.linalg.norm(points - reference_points[label], axis=1)
            row.update(reference_hand_max_error_cm=float(error.max()), reference_hand_mean_error_cm=float(error.mean()))
        (OUT / f'{version}-{label}-contacts.json').write_text(json.dumps(contact, indent=2) + '\n')
        results.append(row)
        print(json.dumps(row), flush=True)

translations = []
for model in models:
    i = model['index']
    old, new = binds['v43'][i], binds['v44b2'][i]
    translations.append(dict(bone=model['bone'], local_translation_change_cm=(Vector(old['translation']) - Vector(new['translation'])).length))
report = dict(scope='Actual Blender skinning of two known source-Eve controls plus a bounded H2 candidate. Calibration-only probe, no clearance solver, no live overlay, no game changes.', input=str(input_path), cases=results, finger_translation_changes=translations)
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

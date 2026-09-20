"""Blender 5.2.2: repose arms without limb scaling; retain every morph."""
import copy
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Vector

import os
OUT = Path(os.environ['CSS_ARM_REST_AUDIT_DIR']).resolve()
if OUT.parent != Path(__file__).resolve().parents[3] / 'work/grip-grounding-v1':
    raise ValueError('Audit directory must be a direct workspace grip-grounding child')
ROOT = OUT.parents[3]
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(MOD / 'tools'))
from export_seduxtress_eve import read_bones, TO_UE
from separate_nextgen_footwear import coordinates, face_records

source = MOD / 'work/CSS_SeduXtress_HandBindV43.blend'
reference = MOD / 'authoring/reference/SKEL_CSS_Base.refskel.json'
output = MOD / 'work/CSS_SeduXtress_ArmRestV44B.blend'
bind_path = output.with_suffix('.bindpose.json')
assert not output.exists() and not bind_path.exists()
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (source, reference)}
bpy.ops.wm.open_mainfile(filepath=str(source))
rig = bpy.data.objects['SKEL_CSS_Base']
bones, old_world = read_bones(MOD / rig['CSS_bind_pose'])
canonical, canonical_world = read_bones(reference)
assert len(bones) == len(canonical) == 379
index = {b['name']: i for i, b in enumerate(bones)}
new_world = [m.copy() for m in old_world]
rotations = {}

def frame(direction, normal):
    x = direction.normalized()
    z = normal-x*normal.dot(x)
    assert z.length > .05
    z.normalize()
    return Matrix((x, z.cross(x).normalized(), z)).transposed()

def palm_normal(world, side):
    forward = world[index[f'middle_01_{side}']].translation-world[index[f'hand_{side}']].translation
    across = world[index[f'index_01_{side}']].translation-world[index[f'pinky_01_{side}']].translation
    return forward.cross(across).normalized()

primary = set()
for side in ('l', 'r'):
    for part, child in (('upperarm', 'lowerarm'), ('lowerarm', 'hand')):
        name = f'{part}_{side}'
        i, j = index[name], index[f'{child}_{side}']
        source_axis = old_world[j].translation-old_world[i].translation
        target_axis = canonical_world[j].translation-canonical_world[i].translation
        rotations[i] = frame(target_axis, palm_normal(canonical_world, side)) @ frame(source_axis, palm_normal(old_world, side)).transposed()
        primary.add(i)
    for name in index:
        if name.endswith('_'+side) and name.startswith(('hand_', 'thumb_', 'index_', 'middle_', 'ring_', 'pinky_')):
            i = index[name]
            rotations[i] = canonical_world[i].to_3x3() @ old_world[i].to_3x3().inverted()
            primary.add(i)

deforms = [Matrix.Identity(4) for _ in bones]
affected = set()
for i, bone in enumerate(bones):
    parent = bone['parent']
    if i not in primary and parent not in affected:
        continue
    affected.add(i)
    a = rotations[i] if i in primary else deforms[parent].to_3x3()
    head = deforms[parent] @ old_world[i].translation
    deforms[i] = a.to_4x4()
    deforms[i].translation = head-a @ old_world[i].translation
    new_world[i] = canonical_world[i].copy() if i in primary else deforms[i] @ old_world[i]
    new_world[i].translation = head

helper_names = {f'{part}_twist_{n:02}_{side}' for part in ('upperarm', 'lowerarm') for n in (1, 2) for side in ('l', 'r')}
for side in ('l', 'r'):
    for part, child in (('upperarm', 'lowerarm'), ('lowerarm', 'hand')):
        start, end = index[f'{part}_{side}'], index[f'{child}_{side}']
        axis = canonical_world[end].translation-canonical_world[start].translation
        for n in (1, 2):
            i = index[f'{part}_twist_{n:02}_{side}']
            fraction = (canonical_world[i].translation-canonical_world[start].translation).dot(axis)/axis.length_squared
            new_world[i] = canonical_world[i].copy()
            new_world[i].translation = new_world[start].translation.lerp(new_world[end].translation, fraction)

lengths = {}
for i in primary:
    parent = bones[i]['parent']
    before = (old_world[i].translation-old_world[parent].translation).length
    after = (new_world[i].translation-new_world[parent].translation).length
    assert abs(before-after) < .0001, (bones[i]['name'], before, after)
    lengths[bones[i]['name']] = dict(before_cm=before, after_cm=after)
assert all(abs(d.to_3x3().determinant()-1) < .00001 for d in deforms)

parts = [o for o in bpy.data.objects if o.type == 'MESH' and any(m.type == 'ARMATURE' and m.object == rig for m in o.modifiers)]
assert bpy.data.objects['Eve Body'] in parts
all_matrices, before_base, reports = {}, {}, {}
for obj in parts:
    n = len(obj.data.vertices)
    basis = TO_UE @ obj.matrix_world
    local = [np.array(basis.inverted() @ d @ basis, dtype=np.float64) for d in deforms]
    matrices = np.zeros((n, 4, 4), dtype=np.float64)
    weights = []
    for v in obj.data.vertices:
        row = {obj.vertex_groups[g.group].name: g.weight for g in v.groups}
        weights.append(row)
        included = [(index[name], w) for name, w in row.items() if name in index and rig.data.bones[name].use_deform and w > 0]
        assert not any(bones[i]['name'] in helper_names and w > .0001 for i, w in included), 'This experiment must preserve the V43 weight control'
        total = sum(w for _, w in included)
        if total:
            matrices[v.index] = sum((local[i]*w/total for i, w in included), start=np.zeros((4, 4)))
        else:
            matrices[v.index] = np.eye(4)
    det = np.linalg.det(matrices[:, :3, :3])
    assert np.min(det) > .4, (obj.name, float(np.min(det)))
    all_matrices[obj.name] = matrices
    before_base[obj.name] = coordinates(obj.data.vertices).copy()
    reports[obj.name] = dict(vertices=n, minimum_blend_determinant=float(np.min(det)), weights=weights,
                             faces=face_records(obj), materials=[m.name if m else None for m in obj.data.materials])

def transform(matrices, points):
    return np.einsum('nij,nj->ni', matrices[:, :3, :3], points)+matrices[:, :3, 3]

# Check our coordinate bake against Blender's independent linear skinning.
rig.hide_viewport = False
rig.hide_set(False)
rig.animation_data_clear()
basis = TO_UE @ rig.matrix_world
for i, bone in enumerate(bones):
    pb = rig.pose.bones[bone['name']]
    assert not pb.constraints
    pb.matrix = basis.inverted() @ deforms[i] @ basis @ pb.bone.matrix_local
    bpy.context.view_layer.update()
body = bpy.data.objects['Eve Body']
test = body.copy()
test.data = body.data.copy()
bpy.context.scene.collection.objects.link(test)
test.shape_key_clear()
test.data.vertices.foreach_set('co', before_base[body.name].ravel())
test.modifiers.clear()
modifier = test.modifiers.new('Independent linear skin check', 'ARMATURE')
modifier.object = rig
modifier.use_deform_preserve_volume = False
test.hide_viewport = False
test.hide_set(False)
bpy.context.view_layer.update()
evaluated = test.evaluated_get(bpy.context.evaluated_depsgraph_get())
actual = coordinates(evaluated.to_mesh().vertices)
expected = transform(all_matrices[body.name], before_base[body.name])
skin_error_cm = float(np.linalg.norm(actual-expected, axis=1).max()*100)
evaluated.to_mesh_clear()
assert skin_error_cm < .001, skin_error_cm
bpy.data.objects.remove(test, do_unlink=True)
for pb in rig.pose.bones:
    pb.matrix_basis = Matrix.Identity(4)
bpy.context.view_layer.update()

summary = {}
for obj in parts:
    matrices = all_matrices[obj.name]
    inverse = np.linalg.inv(matrices)
    unchanged = np.max(np.abs(matrices-np.eye(4)), axis=(1, 2)) < 1e-10
    max_error = 0.0
    keys = obj.data.shape_keys.key_blocks if obj.data.shape_keys else []
    for data in [obj.data.vertices, *[key.data for key in keys]]:
        before = coordinates(data).copy()
        moved = transform(matrices, before).astype(np.float32)
        moved[unchanged] = before[unchanged]
        data.foreach_set('co', moved.ravel())
        roundtrip = transform(inverse, coordinates(data))
        error = float(np.linalg.norm(roundtrip-before, axis=1).max()*100)
        max_error = max(error, max_error)
        assert error < .001, (obj.name, error)
        assert np.array_equal(coordinates(data)[unchanged], before[unchanged])
    assert face_records(obj) == reports[obj.name]['faces']
    assert [m.name if m else None for m in obj.data.materials] == reports[obj.name]['materials']
    assert [{obj.vertex_groups[g.group].name: g.weight for g in v.groups} for v in obj.data.vertices] == reports[obj.name]['weights']
    summary[obj.name] = dict(vertices=len(obj.data.vertices), moved_vertices=int(np.count_nonzero(~unchanged)),
                             shape_keys=len(keys), max_inverse_error_cm=max_error,
                             minimum_blend_determinant=reports[obj.name]['minimum_blend_determinant'])

result = copy.deepcopy(bones)
for i, bone in enumerate(bones):
    if i not in affected:
        continue
    local = new_world[bone['parent']].inverted() @ new_world[i]
    q = local.to_quaternion().normalized()
    result[i]['translation'] = list(local.translation)
    result[i]['rotation'] = [q.x, q.y, q.z, q.w]
bind_path.write_text(json.dumps(result, indent=2)+'\n')
saved, saved_world = read_bones(bind_path)
assert max((a.translation-b.translation).length for a, b in zip(saved_world, new_world, strict=True)) < .001
matrices = {b.name: b.matrix_local.copy() for b in rig.data.bones}
bone_lengths = {b.name: b.length for b in rig.data.bones}
bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
for b in rig.data.edit_bones:
    b.use_connect = False
for i, bone in enumerate(bones):
    b = rig.data.edit_bones[bone['name']]
    b.matrix = basis.inverted() @ saved_world[i] @ old_world[i].inverted() @ basis @ matrices[b.name]
    b.length = bone_lengths[b.name]
bpy.ops.object.mode_set(mode='OBJECT')
for pb in rig.pose.bones:
    pb.matrix_basis = Matrix.Identity(4)
rig['CSS_bind_pose'] = str(bind_path.relative_to(MOD))
rig['CSS_arm_rest_alignment'] = 'V44B diagnostic: anatomical arm directions to working mesh reference, no segment scaling'
bpy.context.view_layer.update()
head_error = max((basis @ rig.data.bones[b['name']].head_local-w.translation).length for b, w in zip(bones, saved_world, strict=True))
assert head_error < .001
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in hashes.items())
bpy.ops.wm.save_as_mainfile(filepath=str(output), check_existing=False)
(OUT/'candidate.json').write_text(json.dumps(dict(output=str(output), bind=str(bind_path), input_hashes=hashes,
    source_files_unchanged=True, bone_count=379, lengths=lengths, parts=summary,
    independent_blender_skin_error_cm=skin_error_cm, bind_head_error_cm=head_error,
    geometry_transforms_ue=[dict(name=b['name'], matrix=[list(r) for r in d]) for b, d in zip(bones, deforms, strict=True)],
    scope='Isolated reposed candidate with unchanged weights. Linear coordinate bake, invertible per vertex. All morphs retained. Not cooked, deployed or visually accepted.'), indent=2)+'\n')
print(json.dumps(dict(output=str(output), independent_skin_error_cm=skin_error_cm, parts=summary)), flush=True)

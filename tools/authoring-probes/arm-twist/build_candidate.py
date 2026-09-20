"""Build isolated V44A twist-binding experiment with Blender 5.2.2."""
import copy
import hashlib
import json
import sys
from pathlib import Path
import bpy
from mathutils import Matrix

import os
OUT = Path(os.environ['CSS_ARM_TWIST_AUDIT_DIR']).resolve()
if OUT.parent != Path(__file__).resolve().parents[3] / 'work/grip-grounding-v1':
    raise ValueError('Audit directory must be a direct workspace grip-grounding child')
ROOT = OUT.parents[3]
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(MOD / 'tools'))
from export_seduxtress_eve import read_bones, TO_UE
from separate_nextgen_footwear import geometry_digest, face_records

source = MOD / 'reference/body-type-variant-EVE/eve_beta10.blend'
baseline = MOD / 'work/CSS_SeduXtress_HandBindV43.blend'
reference = MOD / 'authoring/reference/SKEL_CSS_Base.refskel.json'
output = MOD / 'work/CSS_SeduXtress_ArmTwistV44A.blend'
bind_path = output.with_suffix('.bindpose.json')
assert not output.exists() and not bind_path.exists()
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (source, baseline, reference)}
bpy.ops.wm.open_mainfile(filepath=str(source))
source_parts = {}
for obj in bpy.data.objects:
    if obj.type != 'MESH':
        continue
    ratios = {}
    for vertex in obj.data.vertices:
        groups = {obj.vertex_groups[g.group].name: g.weight for g in vertex.groups}
        row = {}
        for side in ('L', 'R'):
            for part, target, distal in (('upper_arm', 'upperarm', '02'), ('forearm', 'lowerarm', '01')):
                bend = groups.get(f'{part}.bend.twk.{side}', 0)
                twist = groups.get(f'{part}.twist.twk.{side}', 0)
                if twist > .0001:
                    row[f'{target}_{side.lower()}'] = dict(ratio=twist/(bend+twist),
                        target=f'{target}_twist_{distal}_{side.lower()}')
        if row:
            ratios[vertex.index] = row
    if ratios:
        source_parts[obj.name] = dict(count=len(obj.data.vertices), ratios=ratios)

bpy.ops.wm.open_mainfile(filepath=str(baseline))
rig = bpy.data.objects['SKEL_CSS_Base']
bones, old_world = read_bones(MOD / rig['CSS_bind_pose'])
canonical, canonical_world = read_bones(reference)
assert len(bones) == len(canonical) == 379
assert [(b['name'], b['parent']) for b in bones] == [(b['name'], b['parent']) for b in canonical]
index = {b['name']: i for i, b in enumerate(bones)}
new_world = [m.copy() for m in old_world]
pivots = {}
for side in ('l', 'r'):
    for part, child in (('upperarm', 'lowerarm'), ('lowerarm', 'hand')):
        start, end = index[f'{part}_{side}'], index[f'{child}_{side}']
        axis = canonical_world[end].translation-canonical_world[start].translation
        for number in ('01', '02'):
            name = f'{part}_twist_{number}_{side}'
            i = index[name]
            offset = canonical_world[i].translation-canonical_world[start].translation
            fraction = offset.dot(axis)/axis.length_squared
            assert .25 < fraction < .75
            assert not any(b['parent'] == i for b in bones), 'Changing a parent requires explicit descendant treatment'
            new_world[i].translation = old_world[start].translation.lerp(old_world[end].translation, fraction)
            pivots[name] = dict(fraction=fraction, before_cm=list(old_world[i].translation), after_cm=list(new_world[i].translation))

def weights(obj):
    return [{obj.vertex_groups[g.group].name: g.weight for g in v.groups} for v in obj.data.vertices]

changes = {}
for obj in [o for o in bpy.data.objects if o.type == 'MESH']:
    original_geometry = geometry_digest(obj)
    original_faces = face_records(obj)
    original_materials = [m.name if m else None for m in obj.data.materials]
    before = weights(obj)
    data = source_parts.get(obj.name)
    if data:
        assert len(obj.data.vertices) == data['count'], (obj.name, 'Needs explicit source vertex mapping')
        for vertex_id, row in data['ratios'].items():
            for parent, record in row.items():
                current = before[vertex_id].get(parent, 0)
                if current <= .0001:
                    continue
                name = record['target']
                assert before[vertex_id].get(name, 0) == 0
                amount = current*record['ratio']
                remainder = current-amount
                parent_group = obj.vertex_groups[parent]
                if remainder > 1e-8:
                    parent_group.add([vertex_id], remainder, 'REPLACE')
                else:
                    parent_group.remove([vertex_id])
                group = obj.vertex_groups.get(name) or obj.vertex_groups.new(name=name)
                group.add([vertex_id], amount, 'REPLACE')
    after = weights(obj)
    changed = []
    for i, (a, b) in enumerate(zip(before, after, strict=True)):
        if a == b:
            continue
        changed.append(i)
        assert data and i in data['ratios']
        assert abs(sum(a.values())-sum(b.values())) < 2e-7
        assert len([w for w in b.values() if w > .0001]) <= 8
        allowed = set(data['ratios'][i]) | {r['target'] for r in data['ratios'][i].values()}
        assert {n: w for n, w in a.items() if n not in allowed} == {n: w for n, w in b.items() if n not in allowed}
    assert geometry_digest(obj) == original_geometry
    assert face_records(obj) == original_faces
    assert [m.name if m else None for m in obj.data.materials] == original_materials
    changes[obj.name] = dict(vertices=changed, geometry_sha256=original_geometry)

result = copy.deepcopy(bones)
for name in pivots:
    i = index[name]
    result[i]['translation'] = list((new_world[bones[i]['parent']].inverted() @ new_world[i]).translation)
bind_path.write_text(json.dumps(result, indent=2)+'\n')
saved, saved_world = read_bones(bind_path)
assert max((a.translation-b.translation).length for a, b in zip(saved_world, new_world, strict=True)) < .001
assert all(a['rotation'] == b['rotation'] and a['scale'] == b['scale'] for a, b in zip(saved, bones, strict=True))
bpy.ops.object.select_all(action='DESELECT')
rig.hide_viewport = False
rig.hide_set(False)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
inverse = (TO_UE @ rig.matrix_world).inverted()
bpy.ops.object.mode_set(mode='EDIT')
for name in pivots:
    bone = rig.data.edit_bones[name]
    bone.use_connect = False
    delta = inverse @ saved_world[index[name]].translation-bone.head
    bone.head += delta
    bone.tail += delta
bpy.ops.object.mode_set(mode='OBJECT')
rig.animation_data_clear()
for bone in rig.pose.bones:
    assert not bone.constraints
    bone.matrix_basis = Matrix.Identity(4)
rig['CSS_bind_pose'] = str(bind_path.relative_to(MOD))
rig['CSS_arm_twist_policy'] = 'V44A diagnostic: source distal weights to fitted distal game helpers'
bpy.context.view_layer.update()
error = max((TO_UE @ rig.matrix_world @ rig.data.bones[b['name']].head_local-w.translation).length for b, w in zip(bones, saved_world, strict=True))
assert error < .001
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in hashes.items())
bpy.ops.wm.save_as_mainfile(filepath=str(output), check_existing=False)
report = dict(output=str(output), bind=str(bind_path), input_hashes=hashes, source_files_unchanged=True,
              pivots=pivots, changes=changes, source_parts=list(source_parts), max_head_error_cm=error,
              geometry_morphs_uv_materials_unchanged=True, bone_rotations_unchanged=True,
              scope='Isolated V44A hypothesis. No production assets or runtime changes. Visual and engine evaluation still required.')
(OUT / 'candidate.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(dict(output=str(output), changed={k: len(v['vertices']) for k, v in changes.items()}, head_error_cm=error)), flush=True)

"""Blender 5.2: map intact Holiday garments onto the preserved CSS body.

F1 is an offline fitting candidate. No exports, cooked assets or shared skeleton
are written. Surface correspondence preserves loose cloth offsets and supplies
CSS skin weights without relying on Data Transfer to create vertex groups.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform


def coords(data):
    a = np.empty(len(data) * 3, dtype=np.float32)
    data.foreach_get('co', a)
    return a.reshape(-1, 3)


def digest(obj):
    h = hashlib.sha256(coords(obj.data.vertices).tobytes())
    for key in obj.data.shape_keys.key_blocks:
        h.update(key.name.encode())
        h.update(coords(key.data).tobytes())
        h.update(str((key.value, key.mute, key.vertex_group)).encode())
    for v in obj.data.vertices:
        h.update(str([(g.group, g.weight) for g in v.groups]).encode())
    return h.hexdigest()


def frame(a, b, c):
    x = (b - a).normalized()
    z = x.cross(c - a).normalized()
    y = z.cross(x)
    return Matrix((x, y, z)).transposed()


def fitted_points(obj):
    keys = obj.data.shape_keys
    points = coords(keys.key_blocks[0].data) if keys else coords(obj.data.vertices)
    if keys:
        for key in keys.key_blocks[1:]:
            if key.mute or not key.value:
                continue
            delta = coords(key.data) - coords(key.relative_key.data)
            if key.vertex_group:
                group = obj.vertex_groups[key.vertex_group]
                mask = np.asarray([next((g.weight for g in v.groups if g.group == group.index), 0)
                                   for v in obj.data.vertices])
                delta *= mask[:, None]
            points += delta * key.value
    return [obj.matrix_world @ Vector(point) for point in points]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--master', type=Path, required=True)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--evaluated-source', action='store_true')
    a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if a.output.exists() or a.output.with_suffix('.json').exists():
        raise FileExistsError(a.output)
    names = ['Eve Christmas - ' + n for n in ('Dress', 'Arms', 'Legs', 'Panties')]
    bpy.ops.wm.open_mainfile(filepath=str(a.master))
    source = bpy.data.objects['Eve Body']
    source_points = [source.matrix_world @ v.co for v in source.data.vertices]
    evaluated_cloth = {}
    if a.evaluated_source:
        collection = bpy.data.collections.new('Holiday source evaluation')
        bpy.context.scene.collection.children.link(collection)
        for name in ['Eve Body'] + names:
            obj = bpy.data.objects[name]
            collection.objects.link(obj)
            obj.driver_remove('hide_render')
            obj.driver_remove('hide_viewport')
            obj.hide_render = False
            obj.hide_viewport = False
            obj.hide_set(False)
        bpy.context.view_layer.update()
        for obj in bpy.data.objects:
            for modifier in obj.modifiers:
                enabled = modifier.show_render
                obj.driver_remove(modifier.path_from_id() + '.show_viewport')
                obj.driver_remove(modifier.path_from_id() + '.show_render')
                modifier.show_viewport = enabled and modifier.type not in ('SUBSURF', 'PARTICLE_SYSTEM')
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        for name in ['Eve Body'] + names:
            obj = bpy.data.objects[name]
            evaluated = obj.evaluated_get(dg)
            mesh = evaluated.to_mesh()
            assert len(mesh.vertices) == len(obj.data.vertices), (name, 'Evaluated topology changed')
            assert [tuple(p.vertices) for p in mesh.polygons] == [tuple(p.vertices) for p in obj.data.polygons]
            points = [obj.matrix_world @ v.co for v in mesh.vertices]
            if name == 'Eve Body':
                source_points = points
            else:
                evaluated_cloth[name] = points
            evaluated.to_mesh_clear()
    source_faces = [tuple(p.vertices) for p in source.data.polygons]
    source.data.calc_loop_triangles()
    triangles = [tuple(t.vertices) for t in source.data.loop_triangles]
    tree = BVHTree.FromPolygons(source_points, triangles, all_triangles=True)
    bpy.ops.wm.open_mainfile(filepath=str(a.baseline))
    body = bpy.data.objects['Eve Body']
    before = digest(body)
    assert [tuple(p.vertices) for p in body.data.polygons] == source_faces, 'Body topology differs'
    target_points = [body.matrix_world @ v.co for v in body.data.vertices]
    if a.evaluated_source:
        target_points = fitted_points(body)
    rig = bpy.data.objects['SKEL_CSS_Base']
    rig_before = [(b.name, b.parent.name if b.parent else None, list(b.matrix_local)) for b in rig.data.bones]
    bone_names = {b.name for b in rig.data.bones}
    body_groups = {g.index: g.name for g in body.vertex_groups}
    body_weights = [{body_groups[g.group]: g.weight for g in v.groups
                     if body_groups[g.group] in bone_names and g.weight > 0}
                    for v in body.data.vertices]
    rows = []
    for name in names:
        old = bpy.data.objects.get(name)
        if old:
            bpy.data.objects.remove(old, do_unlink=True)
        with bpy.data.libraries.load(str(a.master)) as (_, destination):
            destination.objects = [name]
        obj = destination.objects[0]
        bpy.context.scene.collection.objects.link(obj)
        world = obj.matrix_world.copy()
        original = coords(obj.data.vertices)
        original_keys = {k.name: coords(k.data) for k in obj.data.shape_keys.key_blocks}
        mapped = np.empty_like(original)
        transforms = []
        weights = []
        distances = []
        for index, vertex in enumerate(original):
            point = evaluated_cloth[name][index] if a.evaluated_source else world @ Vector(vertex)
            hit, normal, ti, distance = tree.find_nearest(point)
            ids = triangles[ti]
            src = [source_points[i] for i in ids]
            dst = [target_points[i] for i in ids]
            rotation = frame(*dst) @ frame(*src).transposed()
            location = barycentric_transform(hit, *src, *dst)
            mapped[index] = location + rotation @ (point - hit)
            transforms.append(rotation)
            bary = barycentric_transform(hit, *src, Vector((1,0,0)), Vector((0,1,0)), Vector((0,0,1)))
            combined = {}
            for vi, factor in zip(ids, bary):
                for bone, weight in body_weights[vi].items():
                    combined[bone] = combined.get(bone, 0) + weight * max(0, factor)
            combined = {n: w for n, w in combined.items() if w > 1e-5}
            total = sum(combined.values())
            assert total > 0, (name, index, 'no skin influence')
            weights.append({n: w / total for n, w in combined.items()})
            distances.append(distance)
        for modifier in list(obj.modifiers):
            obj.modifiers.remove(modifier)
        obj.animation_data_clear()
        obj.parent = rig
        obj.matrix_parent_inverse = rig.matrix_world.inverted()
        obj.matrix_world = Matrix.Identity(4)
        obj.vertex_groups.clear()
        groups = {n: obj.vertex_groups.new(name=n) for n in sorted({n for w in weights for n in w})}
        for index, row in enumerate(weights):
            for n, w in row.items():
                groups[n].add([index], w, 'REPLACE')
        obj.data.vertices.foreach_set('co', mapped.ravel())
        matrices = np.asarray([np.asarray(m @ world.to_3x3()) for m in transforms], dtype=np.float32)
        for key in obj.data.shape_keys.key_blocks:
            offsets = original_keys[key.name] - original
            transformed = np.einsum('nij,nj->ni', matrices, offsets)
            key.data.foreach_set('co', (mapped + transformed).ravel())
            if a.evaluated_source:
                key.value = 0
        obj.data.shape_keys.animation_data_clear()
        arm = obj.modifiers.new('CSS skin', 'ARMATURE')
        arm.object = rig
        obj.hide_viewport = False
        obj.hide_render = False
        obj.hide_set(False)
        obj.data.update()
        assert all(v.groups for v in obj.data.vertices)
        rows.append(dict(name=name, vertices=len(mapped), unweighted=0,
                         maximum_source_clearance_m=max(distances),
                         maximum_displacement_m=float(np.linalg.norm(mapped-original, axis=1).max())))
    assert digest(body) == before, 'Body changed'
    assert rig_before == [(b.name, b.parent.name if b.parent else None, list(b.matrix_local)) for b in rig.data.bones]
    a.output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(a.output))
    a.output.with_suffix('.json').write_text(json.dumps(dict(source=str(a.master), baseline=str(a.baseline),
        output=str(a.output), body_sha256=before, body_unchanged=True, rig_unchanged=True, parts=rows,
        evaluated_source=a.evaluated_source,
        scope='Offline surface correspondence candidate. Clearance, morph endpoints, motion and game rendering not yet accepted.'), indent=2)+'\n')
    print('HOLIDAY_CANDIDATE_SAVED', flush=True)


if __name__ == '__main__':
    main()

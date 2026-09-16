"""Export a rest-pose Blender assembly to CSSAuthoring schema 1.

Run with the project's Blender 4.5 / Python 3.11 environment. This writes
uncooked geometry, not a playable CSS package. Bone transforms come from an
explicit Unreal reference skeleton, never reconstructed from GLB bone axes.
"""
import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import re

import bpy
from mathutils import Matrix, Quaternion, Vector

TO_UE = Matrix.Diagonal((100, -100, 100, 1))

# A shape key name has to survive the whole chain: Blender, the mesh JSON, the importer,
# the cooked UMorphTarget and finally a `morph` in a package recipe. CSSImportMesh refuses
# anything the engine would rename, so the exporter refuses it here where the author can
# still fix it by renaming the key.
SHAPE_KEY_NAME = re.compile(r'[A-Za-z0-9_]{1,64}\Z')
# Below this a "moved" vertex is export noise, not a shape. Unreal centimetres.
SHAPE_KEY_EPSILON = 1e-4


def read_bones(path):
    bones = json.loads(path.read_text())
    if not isinstance(bones, list) or not 1 <= len(bones) <= 2048:
        raise ValueError('Expected one to 2048 reference bones')
    names, world = set(), []
    for index, bone in enumerate(bones):
        name, parent = bone['name'], bone['parent']
        if not name or name.casefold() in names or type(parent) is not int:
            raise ValueError('Invalid or duplicate bone name/parent')
        if (index == 0 and parent != -1) or (index > 0 and not 0 <= parent < index):
            raise ValueError('Reference bones must be parent-before-child with one root')
        names.add(name.casefold())
        for key, count in [('translation', 3), ('rotation', 4), ('scale', 3)]:
            if len(bone[key]) != count or not all(math.isfinite(v) for v in bone[key]):
                raise ValueError(f'Invalid {name} {key}')
        x, y, z, w = bone['rotation']
        rotation = Quaternion((w, x, y, z))
        if abs(rotation.magnitude - 1) > .0005 or min(bone['scale']) <= 0:
            raise ValueError(f'Invalid {name} quaternion or scale')
        local = Matrix.LocRotScale(Vector(bone['translation']), rotation, Vector(bone['scale']))
        world.append(world[parent] @ local if parent >= 0 else local)
    return bones, world


def export(args):
    bones, reference_world = read_bones(args.refskel)
    bpy.ops.wm.open_mainfile(filepath=str(args.blend.resolve()))
    rigs = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE']
    if len(rigs) != 1:
        raise ValueError('Assembly must contain exactly one armature')
    rig = rigs[0]
    bone_indices = {b['name']: i for i, b in enumerate(bones)}
    if set(rig.data.bones.keys()) != set(bone_indices):
        raise ValueError('Blender and reference skeleton bone names differ')
    max_error = 0.0
    for bone, reference in zip(bones, reference_world):
        actual = rig.data.bones[bone['name']]
        parent = actual.parent.name if actual.parent else None
        expected_parent = bones[bone['parent']]['name'] if bone['parent'] >= 0 else None
        if parent != expected_parent:
            raise ValueError(f'Parent mismatch: {bone["name"]}')
        error = (TO_UE @ rig.matrix_world @ actual.head_local - reference.translation).length
        max_error = max(max_error, error)
        if error > .05:
            raise ValueError(f'Reference head mismatch: {bone["name"]}: {error:.6f} cm')
    for pose in rig.pose.bones:
        # GLB import introduces about 2.7e-5 scale noise on Genessa's thigh.
        # Reject an authored pose while tolerating that measured roundoff.
        if any(abs(pose.matrix_basis[row][col] - (row == col)) > 1e-4
               for row in range(4) for col in range(4)):
            raise ValueError(f'Non-rest pose on {pose.name}; bake the fit first')
    objects = sorted((o for o in bpy.context.scene.objects if o.type == 'MESH'
                      and any(m.type == 'ARMATURE' and m.object == rig for m in o.modifiers)
                      and (not o.hide_render or args.include_hidden)), key=lambda o: o.name)
    if not objects:
        raise ValueError('No skinned mesh objects selected')
    # UE 5.6.1 MeshUVChannelInfo.h limits this skeletal import path to four.
    # Some GLB exports pad their UV attributes with duplicate channels.
    omitted_uvs = []
    for obj in objects:
        if len(obj.data.uv_layers) <= 4:
            continue
        channels = []
        for index, layer in enumerate(obj.data.uv_layers):
            values = array('f', [0]) * (len(layer.data) * 2)
            layer.data.foreach_get('uv', values)
            if index < 4:
                channels.append(values)
                continue
            matches = [i for i, kept in enumerate(channels) if kept == values]
            if not matches:
                raise ValueError(f'{obj.name}: distinct UV channel {index} exceeds the engine limit of four')
            omitted_uvs.append(dict(object=obj.name, channel=index, identical_to=matches[0]))
    uv_count = min(4, max(len(o.data.uv_layers) for o in objects))
    if uv_count == 0:
        raise ValueError('Assembly has no UV coordinates')
    payload = dict(schema=1, mesh_package=args.mesh_package, skeleton_package=args.skeleton_package,
                   bones=bones, materials=[], points=[], uv_channels=uv_count, wedges=[], faces=[],
                   influences=[], normals=[], colors=[])
    # Shape keys merge by name across objects: a body and a top that both carry "Hips"
    # are one shape the player moves, not two.
    shapes = {}
    report = dict(stage='uncooked authoring interchange, not game validated',
                  front_face_winding='clockwise (Unreal)',
                  reference_sha256=hashlib.sha256(args.refskel.read_bytes()).hexdigest(),
                  max_reference_head_error_cm=max_error, parts=[], discarded_degenerate_faces=0,
                  exported_uv_channels=uv_count, omitted_duplicate_uv_channels=omitted_uvs,
                  omitted_hidden_objects=[o.name for o in bpy.context.scene.objects if o.type == 'MESH' and o.hide_render])
    slots = {}
    for obj in objects:
        if any(m.type != 'ARMATURE' and m.show_viewport for m in obj.modifiers):
            raise ValueError(f'{obj.name}: bake non-armature modifiers first')
        mesh = obj.data
        mesh.calc_loop_triangles()
        if not mesh.uv_layers:
            raise ValueError(f'{obj.name}: missing UVs')
        transform = TO_UE @ obj.matrix_world
        normal_transform = transform.to_3x3().inverted().transposed()
        # Blender's front faces are counter-clockwise; Unreal's are clockwise.
        # The usual Y reflection already changes handedness, so retain that
        # triangle order. Reverse only when the full transform preserves it.
        reverse = transform.to_3x3().determinant() > 0
        triangles = []
        for triangle in mesh.loop_triangles:
            a, b, c = (transform @ mesh.vertices[i].co for i in triangle.vertices)
            if (b-a).cross(c-a).length_squared < 1e-12:
                report['discarded_degenerate_faces'] += 1
            else:
                triangles.append(triangle)
        used_vertices = sorted({i for t in triangles for i in t.vertices})
        points = {v: len(payload['points']) + i for i, v in enumerate(used_vertices)}
        max_influences = 0
        for vertex_index in used_vertices:
            vertex = mesh.vertices[vertex_index]
            position = transform @ vertex.co
            if not all(math.isfinite(v) and abs(v) <= 10000 for v in position):
                raise ValueError(f'{obj.name}: invalid point {vertex_index}')
            payload['points'].append(list(position))
            weights = {}
            for group in vertex.groups:
                if group.weight <= 0:
                    continue
                name = obj.vertex_groups[group.group].name
                if name not in bone_indices:
                    raise ValueError(f'{obj.name}: unknown weighted bone {name}')
                weights[bone_indices[name]] = weights.get(bone_indices[name], 0) + group.weight
            total = sum(weights.values())
            if not weights or len(weights) > 8 or abs(total-1) > .001:
                raise ValueError(f'{obj.name}:{vertex_index}: invalid weights, total={total}, count={len(weights)}')
            max_influences = max(max_influences, len(weights))
            payload['influences'].extend([points[vertex_index], index, weight/total]
                                         for index, weight in sorted(weights.items()))
        if mesh.shape_keys and not args.no_shape_keys:
            basis = mesh.shape_keys.reference_key
            linear = transform.to_3x3()
            for key in mesh.shape_keys.key_blocks:
                if key == basis:
                    continue
                if not SHAPE_KEY_NAME.fullmatch(key.name):
                    raise ValueError(f'{obj.name}: shape key {key.name!r} must be letters, digits '
                                     'and underscores; rename it in Blender')
                moved = shapes.setdefault(key.name, [])
                for vertex_index in used_vertices:
                    # The delta is measured against the point this export actually wrote,
                    # not against the Basis key, so the two can never disagree.
                    delta = linear @ (key.data[vertex_index].co - mesh.vertices[vertex_index].co)
                    if not all(math.isfinite(v) for v in delta):
                        raise ValueError(f'{obj.name}: shape key {key.name} has a non-finite delta')
                    if delta.length < SHAPE_KEY_EPSILON:
                        continue
                    if max(abs(v) for v in delta) > 100:
                        raise ValueError(f'{obj.name}: shape key {key.name} moves a vertex more '
                                         'than 100 cm; check the scene scale')
                    moved.append([points[vertex_index], delta.x, delta.y, delta.z])
        color = mesh.color_attributes.active_color
        corner_map = {}
        face_start = len(payload['faces'])
        for triangle in triangles:
            material = mesh.materials[triangle.material_index]
            if material is None:
                raise ValueError(f'{obj.name}: unassigned material')
            if material.name not in slots:
                slots[material.name] = len(slots)
                payload['materials'].append(material.name)
            corners = list(triangle.loops)
            if reverse:
                corners.reverse()
            face = []
            for loop_index in corners:
                if loop_index not in corner_map:
                    loop = mesh.loops[loop_index]
                    wedge = [points[loop.vertex_index]]
                    for channel in range(uv_count):
                        uv = mesh.uv_layers[min(channel, len(mesh.uv_layers)-1)].data[loop_index].uv
                        wedge.extend((uv.x, 1-uv.y))
                    normal = (normal_transform @ mesh.corner_normals[loop_index].vector).normalized()
                    if normal.length < .9:
                        raise ValueError(f'{obj.name}: invalid corner normal')
                    rgba = (1, 1, 1, 1)
                    if color:
                        if color.domain not in ('CORNER', 'POINT'):
                            raise ValueError(f'{obj.name}: unsupported color domain {color.domain}')
                        rgba = color.data[loop_index if color.domain == 'CORNER' else loop.vertex_index].color
                    corner_map[loop_index] = len(payload['wedges'])
                    payload['wedges'].append(wedge)
                    payload['normals'].append(list(normal))
                    payload['colors'].append([round(max(0, min(1, v))*255) for v in rgba])
                face.append(corner_map[loop_index])
            payload['faces'].append(face + [slots[material.name]])
        report['parts'].append(dict(name=obj.name, points=len(used_vertices),
                                   faces=len(payload['faces'])-face_start, max_influences=max_influences,
                                   unused_vertices=len(mesh.vertices)-len(used_vertices),
                                   uv_channels=len(mesh.uv_layers)))
    # A key that exists but moves nothing is almost always a mistake in the scene, and it
    # would cook into a morph target the player can drag for no effect.
    empty = sorted(name for name, deltas in shapes.items() if not deltas)
    if empty:
        raise ValueError('Shape keys move no vertices: ' + ', '.join(empty))
    if len(shapes) > 64:
        raise ValueError(f'{len(shapes)} shape keys exceeds the importer limit of 64')
    if shapes:
        payload['morph_targets'] = [dict(name=name, deltas=shapes[name]) for name in sorted(shapes)]
    report['shape_keys'] = {name: len(shapes[name]) for name in sorted(shapes)}
    report['counts'] = {key: len(payload[key]) for key in ('bones','materials','points','wedges','faces','influences')}
    if report['discarded_degenerate_faces'] > max(10, len(payload['faces']) // 100):
        raise ValueError('Too many degenerate triangles, inspect source mesh')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(payload, separators=(',', ':'), allow_nan=False) + '\n'
    temp = args.output.with_suffix('.json.tmp')
    temp.write_text(encoded)
    temp.replace(args.output)
    report['output_sha256'] = hashlib.sha256(encoded.encode()).hexdigest()
    args.output.with_suffix('.audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report['counts']), 'reference error:', max_error, 'cm')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('blend', type=Path)
    parser.add_argument('--refskel', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mesh-package', required=True)
    parser.add_argument('--skeleton-package', required=True)
    parser.add_argument('--include-hidden', action='store_true')
    parser.add_argument('--no-shape-keys', action='store_true',
                        help='Leave shape keys out, for a mesh whose keys are working state')
    export(parser.parse_args())

if __name__ == '__main__':
    main()

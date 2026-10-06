"""Blender 5.2.2: replay UE animation samples on Eve's accepted fitted mesh."""
import argparse
import gzip
import hashlib
import json
import math
import sys
import subprocess
from contextlib import ExitStack
from pathlib import Path

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[4]
MOD = ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(MOD/'tools'))
from export_seduxtress_eve import TO_UE, read_bones, EXPORT_SHAPES, LEFT_HAND_CORRECTIVES
from wardrobe_regions import PARTS, COVERED

parser = argparse.ArgumentParser()
parser.add_argument('--motion', type=Path, required=True)
parser.add_argument('--bind', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--stride', type=int, default=2)
parser.add_argument('--video', action='store_true', help='Stream frames to MP4 instead of retaining PNG intermediates')
parser.add_argument('--floor', action='store_true', help='Show the diagnostic Z=0 contact plane')
parser.add_argument('--measure-only', action='store_true', help='Evaluate mesh contact without rendering images')
parser.add_argument('--footwear-only', action='store_true',
                    help='Measure each foot without rebuilding the visible body; requires --measure-only')
parser.add_argument('--pose', choices=('final', 'upstream'), default='final',
                    help='Compare the final pose with the recorded input before secondary motion.')
parser.add_argument('--view', choices=('three-quarter', 'front', 'back', 'side'), default='three-quarter')
parser.add_argument('--camera-scale', type=float, default=2.45,
                    help='Orthographic height in metres, for airborne or extended poses')
parser.add_argument('--camera-height', type=float, default=.95,
                    help='Camera target height in metres')
parser.add_argument('--follow-pelvis', action='store_true',
                    help='Follow translation for long root-motion clips without changing their poses')
parser.add_argument('--position-tolerance-cm', type=float, default=.001,
                    help='Explicit matrix reconstruction tolerance for this diagnostic render')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
assert args.stride > 0
assert not (args.video and args.measure_only)
assert not args.footwear_only or args.measure_only
assert math.isfinite(args.camera_scale) and args.camera_scale > 0
assert math.isfinite(args.camera_height)
assert math.isfinite(args.position_tolerance_cm) and 0 < args.position_tolerance_cm <= .01
assert args.output.resolve().is_relative_to(ROOT) and not args.output.exists()
args.output.mkdir()
blend = MOD/'work/CSS_SeduXtress_HeelSupportsV45C.blend'
blend_hash = hashlib.sha256(blend.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(blend), use_scripts=False)
rig = bpy.data.objects['SKEL_CSS_Base']
bones, bind = read_bones(MOD/rig['CSS_bind_pose'])
expected = json.loads(args.bind.read_text())
assert len(bones) == len(expected) == 379
for actual, entry in zip(bones, expected, strict=True):
    assert actual['name'].lower() == entry['name'].lower() and actual['parent'] == entry['parent']
    assert math.dist(actual['translation'], entry['translation']) < .001
    assert math.dist(actual['scale'], entry['scale']) < .001
    assert min(math.dist(actual['rotation'], entry['rotation']),
               math.dist(actual['rotation'], [-v for v in entry['rotation']])) < .001

parts = ['Eve Body', 'Eve Black Pearl - Suit', 'Eve Black Pearl - Stockings',
         'Eve Black Pearl - Footwear', 'Eve Black Pearl - Heel Supports',
         'Eve Hair - Planet Diving Tail', 'Eve Hair Ponytail Long', 'Eve Hair Sword',
         PARTS['heeled'], PARTS['lining']]
objects = [bpy.data.objects[name] for name in parts]
scene = bpy.data.scenes.new('Eve animation review')
bpy.context.window.scene = scene
for obj in [rig, *objects]:
    scene.collection.objects.link(obj)
    obj.hide_viewport = obj.hide_render = False
    obj.hide_set(False)
rig.animation_data_clear()
for bone in rig.pose.bones:
    assert not bone.constraints
    bone.matrix_basis = Matrix.Identity(4)
shapes = {}
for obj in objects:
    if obj.data.shape_keys:
        obj.data.shape_keys.animation_data_clear()
        for key in obj.data.shape_keys.key_blocks:
            if key.name in EXPORT_SHAPES:
                key.value = 0
        shapes[obj.name] = {k.name: k.value for k in obj.data.shape_keys.key_blocks if k.value and not k.mute}
    for modifier in list(obj.modifiers):
        if modifier.type != 'ARMATURE':
            obj.modifiers.remove(modifier)
        else:
            modifier.show_viewport = modifier.show_render = True
    obj.color = (.53, .57, .62, 1) if obj.name in ('Eve Body', PARTS['lining']) else (.08, .13, .18, 1)
bpy.context.view_layer.update()
body = bpy.data.objects['Eve Body']
covered = [i for i, material in enumerate(body.data.materials)
           if material and material.name in COVERED.values()]
assert len(covered) == len(COVERED)
expected_removed = sum(p.material_index in covered for p in body.data.polygons)
assert expected_removed > 0
body.hide_render = True
filtered = bpy.data.objects.new('Visible body sections', bpy.data.meshes.new('Visible body sections'))
scene.collection.objects.link(filtered)
filtered.matrix_world = body.matrix_world
filtered.color = body.color
basis = TO_UE @ rig.matrix_world
inverse = basis.inverted()
pose_bones = [rig.pose.bones[b['name']] for b in bones]
rest = [b.bone.matrix_local.copy() for b in pose_bones]
rest_inverse = [m.inverted() for m in rest]
bind_inverse = [m.inverted() for m in bind]
indices = {b['name']: i for i, b in enumerate(bones)}
opener = gzip.open if args.motion.suffix == '.gz' else open
with opener(args.motion, 'rt') as stream:
    motion = json.load(stream)
errors = []

scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'OBJECT'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.background_type = 'WORLD'
scene.world = bpy.data.worlds.new('Review background')
scene.world.color = (.055, .055, .055)
if args.floor:
    bpy.ops.mesh.primitive_plane_add(size=20, location=(0, 0, 0))
    bpy.context.object.color = (.14, .15, .16, 1)
scene.render.resolution_x = 900
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
camera = bpy.data.cameras.new('Review camera')
camera.type = 'ORTHO'
camera.ortho_scale = args.camera_scale
view = bpy.data.objects.new('Review camera', camera)
scene.collection.objects.link(view)
scene.camera = view
center = Vector((0, 0, args.camera_height))
direction = {'three-quarter': (4, -2.5, .5), 'front': (1, -5, .35),
             'back': (-1, 5, .35), 'side': (5, 0, .35)}[args.view]
view.location = center + Vector(direction)
view.rotation_euler = (center-view.location).to_track_quat('-Z', 'Y').to_euler()
first_pelvis = None
footwear = [bpy.data.objects[name] for name in
            ('Eve Black Pearl - Footwear', 'Eve Black Pearl - Heel Supports')]
foot_vertices = {}
for obj in footwear:
    groups = {}
    for group in obj.vertex_groups:
        bone = rig.data.bones.get(group.name)
        while bone:
            if bone.name in ('thigh_l', 'thigh_r'):
                groups[group.index] = bone.name[-1]
                break
            bone = bone.parent
    sides = {'l': [], 'r': []}
    for vertex in obj.data.vertices:
        weights = {side: sum(g.weight for g in vertex.groups
                            if groups.get(g.group) == side) for side in sides}
        side = max(weights, key=weights.get)
        assert weights[side] > .5 and weights['r' if side == 'l' else 'l'] < .001, (
            'Ambiguous footwear ownership', obj.name, vertex.index, weights)
        sides[side].append(vertex.index)
    assert all(sides.values()), obj.name
    foot_vertices[obj.name] = sides

body_coordinates = None
visible_vertices = None
with ExitStack() as resources:
    video = None
    if args.video:
        log = resources.enter_context((args.output/'encode.log').open('wb'))
        video = resources.enter_context(subprocess.Popen([
            'ffmpeg', '-hide_banner', '-loglevel', 'error', '-f', 'image2pipe',
            '-framerate', str(motion['fps']/args.stride), '-vcodec', 'png', '-i', '-',
            '-an', '-c:v', 'libx264', '-preset', 'fast', '-crf', '20', '-pix_fmt', 'yuv420p',
            '-movflags', '+faststart', str(args.output/'review.mp4')],
            stdin=subprocess.PIPE, stderr=log))
    for output_frame, frame in enumerate(range(0, len(motion['frames']), args.stride)):
        morphs = motion['frames'][frame].get('morphs', {}) if args.pose == 'final' else {}
        assert set(morphs) <= set(EXPORT_SHAPES) | set(LEFT_HAND_CORRECTIVES), set(morphs)
        for obj in objects:
            if not obj.data.shape_keys:
                continue
            for key in obj.data.shape_keys.key_blocks:
                if key.name in EXPORT_SHAPES or (obj.name == 'Eve Body' and key.name in LEFT_HAND_CORRECTIVES):
                    value = morphs.get(key.name, 0)
                    assert not value or not key.mute, key.name
                    key.value = value
        snapshot = (motion['frames'][frame]['pose']['Snapshot'] if args.pose == 'final'
                    else motion['frames'][frame]['upstream'])
        assert snapshot['bIsValid'] and snapshot['SkeletalMeshName'] == 'SK_BlackPearl2'
        lookup = {n.lower(): t for n, t in zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True)}
        assert len(lookup) == len(snapshot['BoneNames'])
        world, wanted = [], []
        for index, bone in enumerate(bones):
            t = lookup[bone['name'].lower()]
            local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),
                                       Quaternion([t['Rotation'][k] for k in 'WXYZ']),
                                       Vector([t['Scale3D'][k] for k in 'XYZ']))
            world.append(world[bone['parent']] @ local if bone['parent'] >= 0 else local)
            wanted.append(inverse @ world[-1] @ bind_inverse[index] @ basis @ rest[index])
        if args.follow_pelvis:
            pelvis = inverse @ world[indices['pelvis']].translation
            if first_pelvis is None:
                first_pelvis = Vector((0, 0, pelvis.z))
            view.location = center + Vector(direction) + pelvis - first_pelvis
        # Explicit desired parent matrices avoid updating the entire scene per bone.
        for index, bone in enumerate(pose_bones):
            parent = indices[bone.parent.name] if bone.parent else None
            kwargs = dict(parent_matrix=wanted[parent], parent_matrix_local=rest[parent]) if parent is not None else {}
            bone.matrix_basis = bone.bone.convert_local_to_pose(wanted[index], rest[index], invert=True, **kwargs)
        bpy.context.view_layer.update()
        position_error = angle_error = 0
        for index, bone in enumerate(pose_bones):
            actual = basis @ bone.matrix @ rest_inverse[index] @ inverse @ bind[index]
            position_error = max(position_error, (actual.translation-world[index].translation).length)
            angle = actual.to_quaternion().rotation_difference(world[index].to_quaternion()).angle
            angle_error = max(angle_error, min(angle, abs(2*math.pi-angle)))
        assert position_error < args.position_tolerance_cm and angle_error < .001, (frame, position_error, angle_error)
        graph = bpy.context.evaluated_depsgraph_get()
        body_lowest = None
        if not args.footwear_only:
            evaluated = body.evaluated_get(graph)
            if body_coordinates is None:
                mesh = bpy.data.meshes.new_from_object(evaluated, preserve_all_data_layers=True, depsgraph=graph)
                bm = bmesh.new()
                bm.from_mesh(mesh)
                remove = [face for face in bm.faces if face.material_index in covered]
                assert len(remove) == expected_removed
                bmesh.ops.delete(bm, geom=remove, context='FACES_ONLY')
                bm.to_mesh(mesh)
                bm.free()
                previous = filtered.data
                filtered.data = mesh
                bpy.data.meshes.remove(previous)
                visible_vertices = np.array(sorted({i for polygon in mesh.polygons for i in polygon.vertices}))
                body_coordinates = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
            # Armature-only deformation preserves topology; keep the filtered
            # mesh and update positions instead of rebuilding every face.
            assert len(evaluated.data.vertices) == len(filtered.data.vertices)
            evaluated.data.vertices.foreach_get('co', body_coordinates)
            filtered.data.vertices.foreach_set('co', body_coordinates)
            filtered.data.update()
            z_axis = np.asarray(filtered.matrix_world[2], dtype=float)
            body_lowest = float((body_coordinates.reshape(-1, 3)[visible_vertices] @ z_axis[:3] + z_axis[3]).min())
        foot_lowest = {side: math.inf for side in ('l', 'r')}
        contact_vertices = {}
        for original in footwear:
            evaluated = original.evaluated_get(graph)
            assert len(evaluated.data.vertices) == len(original.data.vertices)
            for side, vertices in foot_vertices[original.name].items():
                height, index = min(((evaluated.matrix_world @ evaluated.data.vertices[i].co).z, i)
                                    for i in vertices)
                if height < foot_lowest[side]:
                    foot_lowest[side] = height
                    contact_vertices[side] = dict(object=original.name, vertex=index,
                        weights={original.vertex_groups[g.group].name: g.weight
                                 for g in original.data.vertices[index].groups if g.weight})
        lowest = min(foot_lowest.values())
        errors.append(dict(frame=frame, position_cm=position_error, angle_rad=angle_error,
                           source_frame=motion['frames'][frame].get('source_frame', frame),
                           footwear_lowest_cm=lowest*100,
                           foot_lowest_cm={side: height*100 for side, height in foot_lowest.items()},
                           foot_contact_vertices=contact_vertices,
                           body_lowest_cm=None if body_lowest is None else body_lowest*100,
                           contact_lowest_cm=None if body_lowest is None else min(lowest, body_lowest)*100))
        if not args.measure_only:
            scene.render.filepath = str(args.output/('frame.png' if args.video else f'{output_frame:03d}.png'))
            bpy.ops.render.render(write_still=True)
            if video:
                video.stdin.write((args.output/'frame.png').read_bytes())
    if video:
        video.stdin.close()
        if video.wait(timeout=30) != 0:
            raise RuntimeError('Review encoding failed; inspect encode.log')
assert hashlib.sha256(blend.read_bytes()).hexdigest() == blend_hash
(args.output/'report.json').write_text(json.dumps(dict(
    blend=str(blend), blend_sha256=blend_hash, motion=str(args.motion),
    motion_sha256=hashlib.sha256(args.motion.read_bytes()).hexdigest(),
    sampling=motion.get('sampling', 'Unspecified by capture'),
    fps=motion['fps']/args.stride, parts=parts, shapes=shapes, errors=errors, view=args.view,
    camera_scale=args.camera_scale, camera_height=args.camera_height,
    follow_pelvis=args.follow_pelvis, position_tolerance_cm=args.position_tolerance_cm,
    covered_body_faces_removed=expected_removed, pose=args.pose, measure_only=args.measure_only,
    footwear_only=args.footwear_only, video=args.video, floor=args.floor,
    foot_vertex_counts={name: {side: len(vertices) for side, vertices in sides.items()}
                        for name, sides in foot_vertices.items()},
    wardrobe='Heels and stockings shown; flat feet hidden; heeled stocking feet and footwear lining shown.',
    scope=('UE compressed upstream pose before secondary motion and hand morphs, stationary owner. '
           if args.pose == 'upstream' else
           'UE compressed component pose including secondary motion and hand morphs, stationary owner. '
           if motion.get('compressed_source') else 'Sampled animation pose, no secondary motion. ') +
          'Accepted fitted mesh and wardrobe section selection, solid materials. No gameplay validation.'
), indent=2)+'\n')

"""Blender 5.2.2: replay UE animation samples on Eve's accepted fitted mesh."""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import bmesh
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
parser.add_argument('--pose', choices=('final', 'upstream'), default='final',
                    help='Compare the final pose with the recorded input before secondary motion.')
parser.add_argument('--view', choices=('three-quarter', 'front', 'back', 'side'), default='three-quarter')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
assert args.stride > 0
assert args.output.resolve().is_relative_to(ROOT) and not args.output.exists()
args.output.mkdir()
blend = MOD/'work/CSS_SeduXtress_HeelSupportsV45C.blend'
blend_hash = hashlib.sha256(blend.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(blend))
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
motion = json.loads(args.motion.read_text())
errors = []

scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'OBJECT'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.background_type = 'WORLD'
scene.world = bpy.data.worlds.new('Review background')
scene.world.color = (.055, .055, .055)
scene.render.resolution_x = 900
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
camera = bpy.data.cameras.new('Review camera')
camera.type = 'ORTHO'
camera.ortho_scale = 2.45
view = bpy.data.objects.new('Review camera', camera)
scene.collection.objects.link(view)
scene.camera = view
center = Vector((0, 0, .95))
direction = {'three-quarter': (4, -2.5, .5), 'front': (1, -5, .35),
             'back': (-1, 5, .35), 'side': (5, 0, .35)}[args.view]
view.location = center + Vector(direction)
view.rotation_euler = (center-view.location).to_track_quat('-Z', 'Y').to_euler()

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
    assert position_error < .001 and angle_error < .001, (frame, position_error, angle_error)
    graph = bpy.context.evaluated_depsgraph_get()
    evaluated = body.evaluated_get(graph)
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
    shoes = bpy.data.objects['Eve Black Pearl - Footwear'].evaluated_get(graph)
    heel = bpy.data.objects['Eve Black Pearl - Heel Supports'].evaluated_get(graph)
    lowest = min((o.matrix_world @ v.co).z for o in (shoes, heel) for v in o.data.vertices)
    errors.append(dict(frame=frame, position_cm=position_error, angle_rad=angle_error,
                       footwear_lowest_cm=lowest*100))
    scene.render.filepath = str(args.output/f'{output_frame:03d}.png')
    bpy.ops.render.render(write_still=True)
assert hashlib.sha256(blend.read_bytes()).hexdigest() == blend_hash
(args.output/'report.json').write_text(json.dumps(dict(
    blend=str(blend), blend_sha256=blend_hash, motion=str(args.motion),
    motion_sha256=hashlib.sha256(args.motion.read_bytes()).hexdigest(),
    fps=motion['fps']/args.stride, parts=parts, shapes=shapes, errors=errors, view=args.view,
    covered_body_faces_removed=expected_removed, pose=args.pose,
    wardrobe='Heels and stockings shown; flat feet hidden; heeled stocking feet and footwear lining shown.',
    scope=('UE compressed upstream pose before secondary motion and hand morphs, stationary owner. '
           if args.pose == 'upstream' else
           'UE compressed component pose including secondary motion and hand morphs, stationary owner. '
           if motion.get('compressed_source') else 'UE raw animation pose, no secondary motion. ') +
          'Accepted fitted mesh and wardrobe section selection, solid materials. No gameplay validation.'
), indent=2)+'\n')

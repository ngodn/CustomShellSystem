"""Render the author's masked heel correction beside the original shoes."""
import hashlib
import json
import math
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

root = Path(__file__).resolve().parents[3]
source = root / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/reference/body-type-variant-EVE/eve_beta10.blend'
out = root / 'CustomShellSystem/work/eve26/author-heel-pair'
out.mkdir(exist_ok=False)
digest = hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(source), link=False) as (_, data):
    data.objects = ['Eve Body', 'Eve Extras - Heels']
body, shoe = data.objects
for obj in (body, shoe):
    bpy.context.scene.collection.objects.link(obj)
    obj.hide_viewport = False
    obj.hide_render = False
    obj.hide_set(False)
    for modifier in list(obj.modifiers):
        obj.modifiers.remove(modifier)
    if obj.data.shape_keys:
        obj.data.shape_keys.animation_data_clear()
    obj.color = (.46, .31, .20, 1) if obj == body else (.12, .38, .55, 1)
key = body.data.shape_keys.key_blocks['OutfitHeelsFix']
assert key.vertex_group == 'Feet'
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = 720
scene.render.resolution_y = 960
scene.render.resolution_percentage = 100
scene.display.shading.color_type = 'OBJECT'
scene.display.shading.show_cavity = True
camera = bpy.data.objects.new('camera', bpy.data.cameras.new('camera'))
scene.collection.objects.link(camera)
scene.camera = camera
camera.data.type = 'ORTHO'
camera.data.ortho_scale = .65
target = Vector((0, 0, .05))
camera.location = target + Vector((3, 0, 0))
camera.rotation_euler = (target-camera.location).to_track_quat('-Z', 'Y').to_euler()
for value in (0, 1):
    key.value = value
    bpy.context.view_layer.update()
    scene.render.filepath = str(out / f'side{value}.png')
    bpy.ops.render.render(write_still=True)
poses = json.loads((out.parent / 'bikini-heelpose1/receipt.json').read_text())['shoes']
to_ue = Matrix.Diagonal((100, -100, 100, 1)) @ shoe.matrix_world
from_ue = to_ue.inverted()
for vertex in shoe.data.vertices:
    point = to_ue @ vertex.co
    pose = next(row for row in poses if point.x * row['side'] > 0)
    pivot = Vector(pose['pivot_cm'])
    rotation = Matrix.Rotation(math.radians(pose['angle_degrees']), 3, Vector(pose['axis']))
    vertex.co = from_ue @ (rotation @ (point-pivot) + pivot)
bpy.context.view_layer.update()
scene.render.filepath = str(out / 'paired.png')
bpy.ops.render.render(write_still=True)
assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
(out / 'receipt.json').write_text(json.dumps(dict(source_sha256=digest,
    source_unchanged=True, mask=key.vertex_group,
    scope='Authored shape comparison and isolated rigid shoe pairing; saved other shape values retained, drivers and modifiers disabled in memory. No production body change.'), indent=2)+'\n')

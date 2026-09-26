"""Render exported Holiday material regions without loading or altering the source blend."""
import json
from pathlib import Path
import bpy
from mathutils import Vector

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
payload = json.loads((WORK / 'holiday.mesh.json').read_text())
audit = json.loads((WORK / 'holiday.mesh.audit.json').read_text())
output = WORK / 'section-views'
output.mkdir(exist_ok=False)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
palette = [(1, .5, .04, 1), (.1, .4, .85, 1), (.9, .08, .1, 1),
           (.1, .8, .3, 1), (.7, .12, .85, 1)]
materials = []
for i, name in enumerate(payload['materials']):
    material = bpy.data.materials.new(name)
    material.diffuse_color = palette[(i-16) % 5] if 16 <= i <= 20 else (.32, .32, .32, 1)
    materials.append(material)
start = 0
for part in audit['parts']:
    triangles = payload['faces'][start:start + part['faces']]
    start += part['faces']
    points = sorted({payload['wedges'][w][0] for face in triangles for w in face[:3]})
    lookup = {old: new for new, old in enumerate(points)}
    vertices = [(payload['points'][i][0]/100, -payload['points'][i][1]/100,
                 payload['points'][i][2]/100) for i in points]
    faces = [[lookup[payload['wedges'][w][0]] for w in reversed(face[:3])] for face in triangles]
    mesh = bpy.data.meshes.new(part['name'])
    mesh.from_pydata(vertices, [], faces)
    for mat in materials:
        mesh.materials.append(mat)
    for polygon, triangle in zip(mesh.polygons, triangles, strict=True):
        polygon.material_index = triangle[3]
        polygon.use_smooth = True
    obj = bpy.data.objects.new(part['name'], mesh)
    bpy.context.collection.objects.link(obj)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.light = 'STUDIO'
scene.display.shading.show_cavity = True
scene.render.resolution_x = 800
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
cam = bpy.data.objects.new('Section camera', bpy.data.cameras.new('Section camera'))
bpy.context.collection.objects.link(cam)
scene.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = 1.3
for name, point in [('front', (0, -3, 1.25)), ('back', (0, 3, 1.25)), ('side', (3, 0, 1.25))]:
    cam.location = point
    cam.rotation_euler = (Vector((0, 0, 1.2)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = str(output / (name + '.png'))
    bpy.ops.render.render(write_still=True)

"""Render matching body, rigid hull and overlap views for a regional collision audit."""
import argparse
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.output.mkdir(exist_ok=False)
data = json.loads(a.input.read_text())
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
objects = []
entries = [('body', data['body_positions'], data['body_faces'], (.7, .13, .1, 1))]
entries += [(h['bone'], h['posed_cm'], h['indices'], (.25, .42, .55, 1)) for h in data['hulls']]
for name, points, faces, color in entries:
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([(x/100, -y/100, z/100) for x, y, z in points], [], [list(reversed(f)) for f in faces])
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    mesh.materials.append(mat)
    objects.append(obj)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_cavity = True
scene.render.resolution_x = 720
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
camera = bpy.data.objects.new('Camera', bpy.data.cameras.new('Camera'))
bpy.context.collection.objects.link(camera)
scene.camera = camera
camera.data.type = 'ORTHO'
camera.data.ortho_scale = .9
points = [Vector((x/100, -y/100, z/100)) for h in data['hulls'] if h['bone'] == 'pelvis' for x, y, z in h['posed_cm']]
target = sum(points, Vector())/len(points)
for mode in ('body', 'hulls', 'overlay'):
    for obj in objects:
        obj.hide_render = (mode == 'body' and obj.name != 'body') or (mode == 'hulls' and obj.name == 'body')
    for view, direction in [('rear', (0, 1, 0)), ('side', (1, 0, 0))]:
        camera.location = target+Vector(direction)*2
        camera.rotation_euler = (target-camera.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(a.output/f'{mode}-{view}.png')
        bpy.ops.render.render(write_still=True)
(a.output/'scope.json').write_text(json.dumps({'input': str(a.input), 'scope': 'Matching diagnostic views. Body red, regional rigid hulls blue. Hulls cover only the rest-height band used by the audit, not the whole body. No native asset or simulation.'}, indent=2)+'\n')

"""Render rest-body collision samples as a diagnostic heatmap in Blender."""
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
w = Path(__file__).resolve().parents[2]/'work/eve26'
body = json.loads((w/'body-collider.json').read_text())
query = json.loads(a.input.read_text())['bodies'][0]
samples = query['samples']
positions = query.get('sample_positions_cm', body['positions'])
assert len(samples) == len(body['positions'])
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
mesh = bpy.data.meshes.new('Body distance samples')
mesh.from_pydata([(x/100, -y/100, z/100) for x, y, z in positions], [],
                 [list(reversed(face)) for face in body['indices']])
obj = bpy.data.objects.new('Body distance samples', mesh)
bpy.context.collection.objects.link(obj)
colors = [('within 3mm', (.35, .38, .4, 1)), ('outside 3mm', (1, .12, .015, 1)),
          ('inside 3mm', (.04, .3, 1, 1)), ('cloth query miss', (.9, .02, .7, 1))]
for name, color in colors:
    material = bpy.data.materials.new(name)
    material.diffuse_color = color
    mesh.materials.append(material)
for polygon, face in zip(mesh.polygons, body['indices'], strict=True):
    values = [samples[i][1] for i in face]
    value = sum(values)/3
    polygon.material_index = 3 if any(abs(v) >= 1e6 for v in values) else (1 if value > .3 else 2 if value < -.3 else 0)
    polygon.use_smooth = True
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
camera.data.ortho_scale = .55
target = Vector((0, 0, 1.075))
if query.get('sample_frame', -1) >= 0:
    region = [Vector((x/100, -y/100, z/100)) for i, (x, y, z) in enumerate(positions)
              if 90 <= body['positions'][i][2] <= 125]
    target = sum(region, Vector())/len(region)
    camera.data.ortho_scale = .85
for name, direction in [('front', (0, -1, 0)), ('rear', (0, 1, 0)), ('side', (1, 0, 0))]:
    camera.location = target+Vector(direction)*2
    camera.rotation_euler = (target-camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = str(a.output/(name+'.png'))
    bpy.ops.render.render(write_still=True)
(a.output/'legend.json').write_text(json.dumps({
    'source': str(a.input), 'legend': dict(colors),
    'sample_frame': query.get('sample_frame', -1),
    'scope': 'Body surface colored by face-average vertex distance. This does not render the collider surface or prove containment. Creases and internal surfaces are included.'
}, indent=2)+'\n')

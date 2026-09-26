"""Compare skinned body and approximate lattice-mapped surface in matching views."""
import argparse
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--body', type=Path, help='Optional regional surface JSON')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.output.mkdir(exist_ok=False)
report = json.loads(a.input.read_text())
w = Path(__file__).resolve().parents[2]/'work/eve26'
body = json.loads((a.body or w/'body-collider.json').read_text())
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
objects = []
for label, key in [('body', 'body_positions_cm'), ('mapped', 'mapped_positions_cm')]:
    mesh = bpy.data.meshes.new(label)
    mesh.from_pydata([(x/100, -y/100, z/100) for x,y,z in report[key]], [],
                    [list(reversed(face)) for face in body['indices']])
    obj = bpy.data.objects.new(label, mesh)
    bpy.context.collection.objects.link(obj)
    objects.append(obj)
    for name,color in [('below 3mm',(.35,.38,.4,1)),('3mm to 1cm',(1,.45,.03,1)),('above 1cm',(.85,.04,.02,1))]:
        mat = bpy.data.materials.new(name)
        mat.diffuse_color = color
        mesh.materials.append(mat)
    for poly,face in zip(mesh.polygons,body['indices'],strict=True):
        distance = sum(report['nearest_distance_cm'][i] for i in face)/3
        poly.material_index = 2 if distance > 1 else 1 if distance > .3 else 0
        poly.use_smooth = True
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_cavity = True
scene.render.resolution_x = 720
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
camera = bpy.data.objects.new('Camera',bpy.data.cameras.new('Camera'))
bpy.context.collection.objects.link(camera)
scene.camera = camera
camera.data.type = 'ORTHO'
camera.data.ortho_scale = .85
region = [Vector((x/100,-y/100,z/100)) for i,(x,y,z) in enumerate(report['body_positions_cm'])
          if 90 <= body['positions'][i][2] <= 125]
target = sum(region,Vector())/len(region)
for obj in objects:
    for other in objects:
        other.hide_render = other != obj
    for view,direction in [('front',(0,-1,0)),('rear',(0,1,0)),('side',(1,0,0))]:
        camera.location = target+Vector(direction)*2
        camera.rotation_euler = (target-camera.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath = str(a.output/f'{obj.name}-{view}.png')
        bpy.ops.render.render(write_still=True)
(a.output/'legend.json').write_text(json.dumps({
    'source':str(a.input),'scope':report['scope'],
    'colors':'Grey below 3mm, orange 3mm to 1cm, red above 1cm. Face-average unsigned body-to-mapped distance carried to matching faces in both views. Not a native collider or penetration heatmap.'
},indent=2)+'\n')

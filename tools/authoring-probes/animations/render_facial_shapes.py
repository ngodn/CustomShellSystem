"""Blender 5.2.2: render all exported expression poses on the unchanged Eve source."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--export',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
args.output.mkdir(parents=True,exist_ok=False)
report=json.loads((args.export/'report.json').read_text())
data=json.loads((args.export/'facial-shapes.json').read_text())
source=next(Path(path) for path in report['sources'] if path.endswith('.blend'))
bpy.ops.wm.open_mainfile(filepath=str(source),use_scripts=False)
body=bpy.data.objects['Eve Body']
scene=bpy.data.scenes.new('Expression review')
bpy.context.window.scene=scene
scene.collection.objects.link(body)
body.hide_viewport=body.hide_render=False
body.hide_set(False)
for modifier in list(body.modifiers):
    body.modifiers.remove(modifier)
body.data.shape_keys.animation_data_clear()
keys=body.data.shape_keys.key_blocks
original={name:keys[name].value for name in report['mapping'].values()}
for name in original:
    keys[name].slider_min=-1
    keys[name].mute=False
body.color=(.62,.53,.45,1)
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=True
scene.display.shading.show_cavity=True
scene.world=bpy.data.worlds.new('Expression background')
scene.world.color=(.075,.075,.075)
scene.render.resolution_x=420
scene.render.resolution_y=480
scene.render.resolution_percentage=100
camera=bpy.data.cameras.new('Face review')
camera.type='ORTHO';camera.ortho_scale=.32
obj=bpy.data.objects.new('Face review',camera)
scene.collection.objects.link(obj);scene.camera=obj
center=Vector((0,-.01,1.57))
obj.location=center+Vector((.08,-2,.05))
obj.rotation_euler=(center-obj.location).to_track_quat('-Z','Y').to_euler()
records=[]
for i,pose in enumerate([dict(name='Reset',weights={}),*data['expressions']]):
    for name,value in original.items():
        keys[name].value=value
    for name,weight in pose['weights'].items():
        keys[report['mapping'][name]].value=weight
    bpy.context.view_layer.update()
    filename=f'{i:02d}-{pose["name"]}.png'
    scene.render.filepath=str(args.output/filename)
    bpy.ops.render.render(write_still=True)
    records.append(dict(name=pose['name'],file=filename))
if hashlib.sha256(source.read_bytes()).hexdigest()!=report['sources'][str(source)]:
    raise ValueError('Source blend changed')
(args.output/'index.json').write_text(json.dumps(records,indent=2)+'\n')

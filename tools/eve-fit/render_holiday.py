"""Blender 5.2 neutral-material fitting views; writes images, never saves the blend."""
import argparse
import sys
from pathlib import Path
import bpy
from mathutils import Vector

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--bind',action='store_true',help='Match exporter geometry: saved fit keys, no modifiers')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.output.mkdir(parents=True,exist_ok=False)
visible={'Eve Body','Eve Christmas - Dress','Eve Christmas - Arms','Eve Christmas - Legs','Eve Christmas - Panties','Eve Extras - Heels'}
review=bpy.data.collections.new('Holiday fitting review')
bpy.context.scene.collection.children.link(review)
for obj in bpy.data.objects:
 if obj.type in ('MESH','CURVE','SURFACE'):
  # Original model wardrobe drivers otherwise hide the requested review pieces.
  obj.driver_remove('hide_render')
  obj.driver_remove('hide_viewport')
  obj.hide_render=obj.name not in visible
  if obj.name in visible:
   review.objects.link(obj)
   obj.hide_viewport=False
   obj.hide_set(False)
   if a.bind:
    obj.animation_data_clear()
    for modifier in list(obj.modifiers):
     obj.modifiers.remove(modifier)
    if obj.data.shape_keys:
     obj.data.shape_keys.animation_data_clear()
    obj.show_only_shape_key=False
   obj.color=(0.58,0.36,0.22,1) if obj.name=='Eve Body' else (0.12,0.38,0.55,1)
s=bpy.context.scene
s.render.engine='BLENDER_WORKBENCH'
s.render.resolution_x=720
s.render.resolution_y=960
s.render.resolution_percentage=100
s.render.image_settings.file_format='PNG'
s.display.shading.light='STUDIO'
s.display.shading.color_type='OBJECT'
s.display.shading.show_cavity=True
s.display.shading.cavity_type='BOTH'
s.display.shading.background_type='WORLD'
s.world.color=(0.12,0.12,0.12)
cam=bpy.data.objects.new('Fit review camera',bpy.data.cameras.new('Fit review camera'))
s.collection.objects.link(cam)
cam.data.type='ORTHO'
cam.data.ortho_scale=1.35
s.camera=cam
for name,position in [('front',(0,-3,1.18)),('back',(0,3,1.18)),('side',(3,0,1.18)),('quarter',(2,-3,1.18))]:
 cam.location=Vector(position)
 cam.rotation_euler=(Vector((0,0,1.18))-cam.location).to_track_quat('-Z','Y').to_euler()
 s.render.filepath=str(a.output/(name+'.png'))
 bpy.ops.render.render(write_still=True)
print('HOLIDAY_VIEWS_DONE')

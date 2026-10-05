"""Blender 5.2.2: render all exported expression poses on the unchanged Eve source."""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from prepare_performance_curves import evaluate

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--export',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--view',choices=('front','three-quarter','profile'),default='front')
parser.add_argument('--performance',type=int,help='Render this exported performance instead of static expressions')
parser.add_argument('--fps',type=int,default=10,help='Diagnostic performance sampling rate')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
if not 1 <= args.fps <= 60:
    raise ValueError('Use 1 to 60 diagnostic samples per second')
args.output.mkdir(parents=True,exist_ok=False)
report=json.loads((args.export/'report.json').read_text())
data=json.loads((args.export/'facial-shapes.json').read_text())
poses=[dict(name='Reset',weights={}),*data['expressions']]
if args.performance is not None:
    if not 0 <= args.performance < len(data.get('performances',[])):
        raise ValueError('Unknown performance')
    performance=data['performances'][args.performance]
    duration=performance['duration_seconds']
    times=[i/args.fps for i in range(math.ceil(duration*args.fps))]+[duration]
    poses=[dict(name=f'frame-{i:04d}',seconds=t,
                weights={curve['CurveName']:evaluate(curve,t) for curve in performance['curves']})
           for i,t in enumerate(times)]
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
    keys[name].slider_min=-4
    keys[name].slider_max=4
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
offset={'front':(.08,-2,.05),'three-quarter':(1.3,-1.5,.05),'profile':(2,0,.05)}[args.view]
obj.location=center+Vector(offset)
obj.rotation_euler=(center-obj.location).to_track_quat('-Z','Y').to_euler()
records=[]
for i,pose in enumerate(poses):
    for name,value in original.items():
        keys[name].value=value
    for name,weight in pose['weights'].items():
        if not math.isfinite(weight) or abs(weight)>4:
            raise ValueError('Unsupported facial diagnostic weight')
        keys[report['mapping'][name]].value=weight
        if abs(keys[report['mapping'][name]].value-weight)>1e-6:
            raise ValueError('Blender clamped a source facial weight')
    bpy.context.view_layer.update()
    filename=f'{i:02d}-{pose["name"]}.png'
    scene.render.filepath=str(args.output/filename)
    bpy.ops.render.render(write_still=True)
    record=dict(name=pose['name'],file=filename,view=args.view)
    if 'seconds' in pose:
        record['seconds']=pose['seconds']
    records.append(record)
if hashlib.sha256(source.read_bytes()).hexdigest()!=report['sources'][str(source)]:
    raise ValueError('Source blend changed')
(args.output/'index.json').write_text(json.dumps(records,indent=2)+'\n')

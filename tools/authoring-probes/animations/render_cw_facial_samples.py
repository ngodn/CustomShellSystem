"""Blender 5.2.2: neutral shaded close-ups of sampled source facial geometry."""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector


parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,required=True)
parser.add_argument('--samples',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--label',action='append',help='Sample label to render; repeat for an explicit review set')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
if args.output.exists() or bpy.context.preferences.filepaths.use_scripts_auto_execute:
    raise ValueError('Use a new output directory and --disable-autoexec')
with args.source.open('rb') as stream:
    source_hash=hashlib.file_digest(stream,'sha256').hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(args.source.resolve()),use_scripts=False)
topologies={}
with np.load(args.samples/'neutral.npz') as neutral:
    for name in neutral.files:
        obj=bpy.data.objects[name]
        # Omit only the optical shells, which obscure the eyeball in an opaque
        # diagnostic material. The actual asset and sampled points are intact.
        polygons=[p for p in obj.data.polygons if obj.data.materials[p.material_index].name not in ('Cornea-2','EyeMoisture-2')]
        topologies[name]=(neutral[name].copy()/100,[list(p.vertices) for p in polygons],
                          [obj.data.materials[p.material_index].name for p in polygons])
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
materials={}
parts={}
for name,(points,faces,names) in topologies.items():
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(points.tolist(),[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj);parts[name]=obj
    slots={}
    for material in names:
        if material in slots:continue
        if material not in materials:
            m=bpy.data.materials.new(material)
            m.diffuse_color=(.018,.024,.03,1) if material in ('Pupils-2','Eyelashes-1') else (.57,.63,.69,1)
            materials[material]=m
        slots[material]=len(mesh.materials);mesh.materials.append(materials[material])
    for poly,material in zip(mesh.polygons,names):
        poly.material_index=slots[material];poly.use_smooth=True
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=True
scene.display.shading.show_cavity=True
scene.display.shading.background_type='WORLD'
scene.world=bpy.data.worlds.new('Facial audit background');scene.world.color=(.06,.065,.07)
scene.render.resolution_x=600;scene.render.resolution_y=700;scene.render.resolution_percentage=100
camera=bpy.data.cameras.new('Facial audit camera');camera.type='ORTHO';camera.ortho_scale=.34
view=bpy.data.objects.new('Facial audit camera',camera);scene.collection.objects.link(view);scene.camera=view
center=Vector((0,-.01,1.645))
args.output.mkdir(parents=True)
labels=args.label or ['neutral','facs_jnt_JawOpen-0.5','facs_jnt_JawOpen-1',
        'facs_jnt_EyeBlinkLeft-0.5','facs_jnt_EyeBlinkLeft-1',
        'facs_jnt_EyeBlinkRight-0.5','facs_jnt_EyeBlinkRight-1']
if any(not re.fullmatch(r'[A-Za-z0-9_-]+(?:\.[0-9]+)?',label) for label in labels):
    raise ValueError('Invalid sample label')
records=[]
for label in labels:
    with np.load(args.samples/(label+'.npz')) as data:
        for name,obj in parts.items():
            obj.data.vertices.foreach_set('co',(data[name]/100).astype(np.float32).ravel());obj.data.update()
    for direction,offset in [('front',(0,-2,0)),('quarter',(.8,-2,0))]:
        view.location=center+Vector(offset);view.rotation_euler=(center-view.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(args.output/(label+'-'+direction+'.png'))
        bpy.ops.render.render(write_still=True)
        records.append(scene.render.filepath)
with args.source.open('rb') as stream:
    if hashlib.file_digest(stream,'sha256').hexdigest()!=source_hash:
        raise ValueError('Source file changed')
(args.output/'report.json').write_text(json.dumps(dict(source_sha256=source_hash,frames=records,
    scope='Neutral source geometry close-ups. Optical shells omitted for inspection; no game shaders or runtime acceptance.'),indent=2)+'\n')

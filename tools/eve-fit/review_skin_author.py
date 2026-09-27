"""Inspect the author's Skin Suit panels without altering the source blend."""
import hashlib
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector

root=Path(__file__).resolve().parents[3]
mod=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0,str(mod/'gemini-work'))
from export_variant_clean import fitted_mesh,TO_UE
source=mod/'reference/body-type-variant-EVE/eve_beta10.blend'
out=root/'CustomShellSystem/work/eve26/skin-author-panels'
out.mkdir(exist_ok=False)
digest=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(source),link=False) as (_,dst):
    dst.objects=['Eve Skin Suit - Suit Complete']
obj=dst.objects[0]
bpy.context.scene.collection.objects.link(obj)
mesh,_,active=fitted_mesh(obj)
transform=TO_UE@obj.matrix_world
points=[transform@v.co for v in mesh.vertices]
panels=[]
for slot,material in enumerate(mesh.materials):
    faces=[list(p.vertices) for p in mesh.polygons if p.material_index==slot]
    used=sorted({i for face in faces for i in face})
    if not used:continue
    index={v:i for i,v in enumerate(used)}
    data=bpy.data.meshes.new(material.name)
    data.from_pydata([(points[i].x/100,-points[i].y/100,points[i].z/100) for i in used],[],
                     [[index[i] for i in face] for face in faces])
    panel=bpy.data.objects.new(material.name,data)
    bpy.context.scene.collection.objects.link(panel)
    panel.color=(.58,.36,.22,1) if 'skin' in material.name.lower() else (.12,.38,.55,1)
    for polygon in data.polygons:polygon.use_smooth=True
    panels.append(dict(material=material.name,points=len(used),faces=len(faces),
                       bounds_cm=[[min(points[i][axis] for i in used),max(points[i][axis] for i in used)] for axis in range(3)]))
obj.hide_render=True
s=bpy.context.scene
s.render.engine='BLENDER_WORKBENCH'
s.render.resolution_x,s.render.resolution_y,s.render.resolution_percentage=720,960,100
s.display.shading.color_type='OBJECT'
s.display.shading.show_cavity=True
cam=bpy.data.objects.new('Review camera',bpy.data.cameras.new('Review camera'))
s.collection.objects.link(cam);s.camera=cam;cam.data.type='ORTHO'
verts=[v.co for o in s.objects if o.type=='MESH' and not o.hide_render for v in o.data.vertices]
low=Vector([min(v[i] for v in verts) for i in range(3)])
high=Vector([max(v[i] for v in verts) for i in range(3)])
target=(low+high)/2
cam.data.ortho_scale=(high.z-low.z)*1.15
for label,direction in [('front',(0,-1,0)),('back',(0,1,0)),('side',(1,0,0))]:
    cam.location=target+Vector(direction)*3
    cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    s.render.filepath=str(out/f'{label}.png');bpy.ops.render.render(write_still=True)
assert hashlib.sha256(source.read_bytes()).hexdigest()==digest
(out/'receipt.json').write_text(json.dumps(dict(source_sha256=digest,source_unchanged=True,
    panels=panels,active_shapes=active,scope='Original garment only; no armature or modifier deformation. Not a CSS fit check.'),indent=2)+'\n')

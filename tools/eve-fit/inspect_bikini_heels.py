"""Compare author heel geometry with and without its saved rig deformation."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
root=Path(__file__).resolve().parents[3]
source=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/reference/body-type-variant-EVE/eve_beta10.blend'
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);out=args.output;out.mkdir(exist_ok=False)
sha=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(source),link=False) as (_,dst):dst.objects=['Eve Extras - Heels']
heel=dst.objects[0]
for obj in list(bpy.data.objects):
 if not obj.users_collection:bpy.context.scene.collection.objects.link(obj)
arm=next(m for m in heel.modifiers if m.type=='ARMATURE');rig=arm.object;assert rig
arm.show_viewport=True;heel.hide_viewport=False;heel.hide_set(False);rig.hide_viewport=False;rig.hide_set(False)
used={heel.vertex_groups[g.group].name for v in heel.data.vertices for g in v.groups if g.weight>0}
metadata=dict(rig=rig.name,pose_position=rig.data.pose_position,modifier=dict(name=arm.name,vertex_group=arm.vertex_group,invert_vertex_group=arm.invert_vertex_group,use_deform_preserve_volume=arm.use_deform_preserve_volume),weighted_groups=sorted(used),bones=[])
metadata['heel_controls']=[]
for owner in [rig,rig.data,*rig.pose.bones]:
 for key in owner.keys():
  if 'heel' in key.lower():metadata['heel_controls'].append(dict(owner=owner.name,key=key,value=str(owner[key])))
metadata['heel_bones']=[b.name for b in rig.pose.bones if 'heel' in b.name.lower()]
for name in sorted(used):
 bone=rig.pose.bones.get(name)
 if bone:metadata['bones'].append(dict(name=name,scale=list(bone.scale),matrix_basis=[list(row) for row in bone.matrix_basis],constraints=[c.type for c in bone.constraints]))
def evaluate(enabled):
 arm.show_viewport=enabled;bpy.context.view_layer.update()
 deps=bpy.context.evaluated_depsgraph_get();obj=heel.evaluated_get(deps);mesh=obj.to_mesh()
 points=np.asarray([heel.matrix_world@v.co for v in mesh.vertices]);faces=[list(p.vertices) for p in mesh.polygons];obj.to_mesh_clear()
 return points,faces
raw,faces=evaluate(False);posed,posed_faces=evaluate(True);assert faces==posed_faces and raw.shape==posed.shape
rows=[]
for name,points in [('raw',raw),('posed',posed)]:
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(points.tolist(),[],faces)
 obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj);obj.color=(.12,.38,.55,1)
 for p in mesh.polygons:p.use_smooth=True
 rows.append(dict(mode=name,bounds_cm=[(points.min(axis=0)*100).tolist(),(points.max(axis=0)*100).tolist()]))
for obj in bpy.context.scene.objects:obj.hide_render=obj.name not in ('raw','posed')
s=bpy.context.scene;s.render.engine='BLENDER_WORKBENCH';s.render.resolution_x=720;s.render.resolution_y=960;s.render.resolution_percentage=100;s.display.shading.color_type='OBJECT';s.display.shading.show_cavity=True
cam=bpy.data.objects.new('camera',bpy.data.cameras.new('camera'));s.collection.objects.link(cam);s.camera=cam;cam.data.type='ORTHO'
all_points=np.concatenate([raw,posed]);low=Vector(all_points.min(axis=0));high=Vector(all_points.max(axis=0));target=(low+high)/2;cam.data.ortho_scale=max(high.z-low.z,high.y-low.y)*1.3
cam.location=target+Vector((3,0,0));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
for name in ('raw','posed'):
 bpy.data.objects['raw'].hide_render=name!='raw';bpy.data.objects['posed'].hide_render=name!='posed';s.render.filepath=str(out/f'{name}.png');bpy.ops.render.render(write_still=True)
assert hashlib.sha256(source.read_bytes()).hexdigest()==sha
(out/'report.json').write_text(json.dumps(dict(source_sha256=sha,source_unchanged=True,metadata=metadata,modes=rows,max_displacement_cm=float(np.linalg.norm(posed-raw,axis=1).max()*100),scope='Loaded heel and rig dependencies, saved pose, armature on/off. Other scene-driven controls may require full-scene evaluation.'),indent=2)+'\n')
print(json.dumps(rows),flush=True)

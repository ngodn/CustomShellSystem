"""Blender 5.2 UV-placement review of F14 palettes, not game material validation."""
import json
from pathlib import Path
import bpy
from mathutils import Vector

root=Path(__file__).resolve().parents[2]
work=root/'work/eve26'
out=work/'p14colors/model';out.mkdir(exist_ok=False)
data=json.loads((work/'planet-f14-import/planet.mesh.json').read_text())
proof=json.loads((work/'p14colors/verification.json').read_text())
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.color_type='TEXTURE'
scene.display.shading.light='STUDIO'
scene.display.shading.show_shadows=True
scene.display.shading.background_type='WORLD'
scene.world.color=(.12,.12,.12)
scene.render.resolution_x=600;scene.render.resolution_y=850;scene.render.resolution_percentage=100
faces=[f for f in data['faces'] if f[3] not in (18,19,20,21,22,23,24,25)]
# Undo the exporter winding reflection for a Blender review.
faces=[[*reversed(f[:3]),f[3]] for f in faces]
mesh=bpy.data.meshes.new('F14 review')
mesh.from_pydata([(p[0]/100,-p[1]/100,p[2]/100) for p in data['points']],[],[[data['wedges'][w][0] for w in f[:3]] for f in faces])
obj=bpy.data.objects.new('Accepted F14',mesh);scene.collection.objects.link(obj)
uv=mesh.uv_layers.new()
for poly,face in zip(mesh.polygons,faces):
    poly.material_index=face[3];poly.use_smooth=True
    for loop,w in zip(poly.loop_indices,face[:3]):uv.data[loop].uv=(data['wedges'][w][1],1-data['wedges'][w][2])
for name in data['materials']:
    mat=bpy.data.materials.new(name);mat.diffuse_color=(.32,.32,.32,1);mat.use_nodes=True
    node=mat.node_tree.nodes.new('ShaderNodeTexImage');mat.node_tree.nodes.active=node
    mesh.materials.append(mat)
cam=bpy.data.objects.new('Review camera',bpy.data.cameras.new('Review camera'));scene.collection.objects.link(cam)
cam.data.type='ORTHO';cam.data.ortho_scale=1.95;scene.camera=cam
source=root.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/textures_staged'
for palette in ['original']+[p['id'] for p in proof['palettes']]:
    for atlas,slots in [('PD_Suit',[16]),('PD_Acc',[17,26,27,28])]:
        path=source/atlas/'T_ShellKeeper_Hair_01_BC.png' if palette=='original' else work/'p14colors'/f'{palette}-{atlas.lower()}.png'
        image=bpy.data.images.load(str(path),check_existing=True)
        for slot in slots:mesh.materials[slot].node_tree.nodes.active.image=image
    for name,pos in [('front',(2,-4,1.1)),('back',(-2,4,1.1))]:
        cam.location=pos;cam.rotation_euler=(Vector((0,0,.9))-cam.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(out/f'{palette}-{name}.png');bpy.ops.render.render(write_still=True)
(out/'verification.json').write_text(json.dumps(dict(source='planet-f14-import/planet.mesh.json',palettes=6,views=2,
    scope='Workbench texture UV-placement review; neutral body, hair omitted. Game shader, alpha and physics are not represented.'),indent=2)+'\n')

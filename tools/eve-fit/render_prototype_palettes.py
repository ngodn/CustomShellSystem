"""Blender 5.2 UV-placement review of fitted outfit palettes, not game material validation."""
import argparse
import sys
import json
from pathlib import Path
import bpy
from mathutils import Vector

root=Path(__file__).resolve().parents[2]
work=root/'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--outfit',choices=('prototype','skin','bikini','knit','alice'),default='prototype')
a=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
skin=a.outfit=='skin'
bikini=a.outfit=='bikini'
knit=a.outfit=='knit'
alice=a.outfit=='alice'
folder=work/('a3trial' if alice else 'k4trial' if knit else 'b1colors' if bikini else 's16colors' if skin else 'p14colors')
meshpath=work/('alice-import4/alice.mesh.json' if alice else 'knit-w2/knit.mesh.json' if knit else 'bikini-import1/bikini.mesh.json' if bikini else 'skin-f16-import/skin.mesh.json' if skin else 'planet-f14-import/planet.mesh.json')
out=folder/'model';out.mkdir(exist_ok=False)
data=json.loads(meshpath.read_text())
proof=json.loads((folder/'verification.json').read_text())
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.color_type='TEXTURE'
scene.display.shading.light='STUDIO'
scene.display.shading.show_shadows=True
scene.display.shading.background_type='WORLD'
scene.world.color=(.12,.12,.12)
scene.render.resolution_x=600;scene.render.resolution_y=850;scene.render.resolution_percentage=100
hidden=(23,24,25) if alice else (25,26,27) if knit else (29,30,31) if bikini else (17,18,19,20,21) if skin else (18,19,20,21,22,23,24,25)
faces=[f for f in data['faces'] if f[3] not in hidden]
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
    entries=((entry['atlas'],entry) for entry in proof['atlases']) if (knit or alice) else proof['atlases'].items()
    for atlas,entry in entries:
        slots=entry['slots']
        key=entry['control'] if (knit or alice) else atlas.lower()
        path=source/('BK_Trim' if atlas=='BK_Metal' else atlas)/'T_ShellKeeper_Hair_01_BC.png' if palette=='original' else folder/f'{palette}-{key}.png'
        image=bpy.data.images.load(str(path),check_existing=True)
        for slot in slots:mesh.materials[slot].node_tree.nodes.active.image=image
    for name,pos in [('front',(2,-4,1.1)),('back',(-2,4,1.1))]:
        cam.location=pos;cam.rotation_euler=(Vector((0,0,.9))-cam.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(out/f'{palette}-{name}.png');bpy.ops.render.render(write_still=True)
(out/'verification.json').write_text(json.dumps(dict(source=str(meshpath.relative_to(work)),palettes=6,views=2,
    scope='Workbench texture UV-placement review; neutral body, hair omitted. Game shader, alpha and physics are not represented.'),indent=2)+'\n')

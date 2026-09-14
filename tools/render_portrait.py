#!/usr/bin/env python3
"""Render an actual extracted mesh head portrait with Blender bpy 4.5 (Python 3.11).

Input is the output directory of tools/MeshExport, including its material JSON
and texture PNGs. This is an optional authoring tool, not a runtime dependency.
"""
import argparse
import json
import math
from pathlib import Path

import bpy
from mathutils import Vector


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('model',type=Path)
    parser.add_argument('output',type=Path)
    parser.add_argument('--yaw',type=float,default=12)
    parser.add_argument('--scale',type=float,default=.56,help='Portrait height in meters')
    parser.add_argument('--height',type=float,default=0,help='Focus offset above the head bone')
    parser.add_argument('--samples',type=int,default=64)
    args=parser.parse_args()
    root=args.model.resolve()
    glbs=list(root.rglob('*.glb'))
    if len(glbs)!=1:
        raise ValueError('Export exactly one skeletal mesh per portrait directory')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(glbs[0]))
    meshes=[o for o in bpy.data.objects if o.type=='MESH' and len(o.data.vertices)>100]
    if len(meshes)!=1:
        raise ValueError('Expected one imported body mesh')
    mesh=meshes[0]
    for obj in list(bpy.data.objects):
        if obj.type=='MESH' and obj!=mesh:
            bpy.data.objects.remove(obj,do_unlink=True)
    rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
    bone=next((b for b in rig.data.bones if b.name.lower()=='head'),None)
    if bone is None:
        raise ValueError('No head bone. Supply a humanoid mesh for automatic framing')
    center=rig.matrix_world@bone.head_local+Vector((0,0,args.height))

    def texture_node(tree,path,linear=False):
        package=path.split('.',1)[0]
        file=root/('MortalShell2/Content/'+package.removeprefix('/Game/')+'.png')
        if not file.is_file():
            return None
        image=bpy.data.images.load(str(file),check_existing=True)
        if linear: image.colorspace_settings.name='Non-Color'
        node=tree.nodes.new('ShaderNodeTexImage');node.image=image
        return node

    for material in mesh.data.materials:
        matches=list(root.rglob(material.name+'.json'))
        material.use_nodes=True
        tree=material.node_tree;tree.nodes.clear()
        shader=tree.nodes.new('ShaderNodeBsdfPrincipled')
        output=tree.nodes.new('ShaderNodeOutputMaterial')
        tree.links.new(shader.outputs['BSDF'],output.inputs['Surface'])
        shader.inputs['Roughness'].default_value=.52
        shader.inputs['Base Color'].default_value=(.08,.065,.05,1)
        if not matches: continue
        candidates=[json.loads(p.read_text()) for p in matches]
        data=next((j for j in candidates if isinstance(j,dict) and 'Textures' in j),{})
        textures=data.get('Textures',{})
        if not textures and data.get('Parameters',{}).get('IsTranslucent'):
            # Runtime smoke cards have no baked color/mask to render offline.
            # Keep them transparent instead of displaying opaque placeholder geometry.
            shader.inputs['Alpha'].default_value=0
            continue
        color=textures.get('PM_Diffuse')
        if color:
            node=texture_node(tree,color)
            if node:
                tree.links.new(node.outputs['Color'],shader.inputs['Base Color'])
                if data.get('Parameters',{}).get('BlendMode')==1:
                    tree.links.new(node.outputs['Alpha'],shader.inputs['Alpha'])
        normal=textures.get('PM_Normals')
        if normal:
            node=texture_node(tree,normal,True)
            if node:
                split=tree.nodes.new('ShaderNodeSeparateColor')
                combine=tree.nodes.new('ShaderNodeCombineColor')
                invert=tree.nodes.new('ShaderNodeMath');invert.operation='SUBTRACT';invert.inputs[0].default_value=1
                tree.links.new(node.outputs['Color'],split.inputs['Color'])
                tree.links.new(split.outputs['Red'],combine.inputs['Red'])
                tree.links.new(split.outputs['Green'],invert.inputs[1])
                tree.links.new(invert.outputs[0],combine.inputs['Green'])
                tree.links.new(split.outputs['Blue'],combine.inputs['Blue'])
                bump=tree.nodes.new('ShaderNodeNormalMap');bump.inputs['Strength'].default_value=.6
                tree.links.new(combine.outputs['Color'],bump.inputs['Color'])
                tree.links.new(bump.outputs['Normal'],shader.inputs['Normal'])
        masks=textures.get('BRM non VT') or textures.get('PM_SpecularMasks')
        if masks:
            node=texture_node(tree,masks,True)
            if node:
                split=tree.nodes.new('ShaderNodeSeparateColor');tree.links.new(node.outputs['Color'],split.inputs['Color'])
                tree.links.new(split.outputs['Green'],shader.inputs['Roughness'])
                tree.links.new(split.outputs['Blue'],shader.inputs['Metallic'])

    def point_at(obj,target):
        obj.rotation_euler=(target-obj.location).to_track_quat('-Z','Y').to_euler()

    scene=bpy.context.scene
    camera=bpy.data.objects.new('Portrait camera',bpy.data.cameras.new('Portrait camera'))
    scene.collection.objects.link(camera)
    angle=math.radians(args.yaw)
    camera.location=center+Vector((math.sin(angle)*2,-math.cos(angle)*2,.035))
    camera.data.type='ORTHO';camera.data.ortho_scale=args.scale;point_at(camera,center)
    scene.camera=camera
    for name,offset,power,size,color in [
        ('Key',(-1,-1.8,1.1),180,1.2,(1,.86,.7)),
        ('Fill',(1.4,-1,.4),90,1.5,(.65,.77,1)),
        ('Rim',(.2,1,.9),240,.8,(1,.7,.4))]:
        light=bpy.data.lights.new(name,'AREA');light.energy=power;light.shape='DISK';light.size=size;light.color=color
        obj=bpy.data.objects.new(name,light);scene.collection.objects.link(obj)
        obj.location=center+Vector(offset);point_at(obj,center)
    scene.world=bpy.data.worlds.new('Charcoal studio');scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.012,.015,.02,1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value=.35
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=args.samples
    scene.cycles.use_denoising=True
    scene.render.threads_mode='FIXED';scene.render.threads=8
    scene.render.resolution_x=512;scene.render.resolution_y=512;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
    scene.view_settings.view_transform='AgX'
    args.output.parent.mkdir(parents=True,exist_ok=True)
    scene.render.filepath=str(args.output.resolve())
    bpy.ops.render.render(write_still=True)
    print(f'Actual mesh portrait: {args.output}')


if __name__=='__main__':
    main()

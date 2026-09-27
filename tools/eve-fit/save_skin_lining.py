"""Save and freshly reload the tested ankle lining as a standalone authoring library."""
import hashlib,json
from pathlib import Path
import bpy
from mathutils import Vector
w=Path(__file__).resolve().parents[2]/'work/eve26'
source=w/'skin-lining4/skin.mesh.json';data=json.loads(source.read_text())
audit=json.loads(source.with_suffix('.audit.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest()==audit['output_sha256']
part=audit['parts'][-1];assert part['name']=='Eve Skin Suit - Footwear Lining'
begin=len(data['points'])-part['points'];records=data['faces'][-part['faces']:]
output=w/'skin-foot-lining.blend';assert not output.exists()
bpy.ops.wm.read_factory_settings(use_empty=True)
def converted(p):return (p[0]/100,-p[1]/100,p[2]/100)
positions=[converted(p) for p in data['points'][begin:]]
faces=[[data['wedges'][i][0]-begin for i in f[:3]] for f in records]
mesh=bpy.data.meshes.new(part['name']);mesh.from_pydata(positions,[],faces)
obj=bpy.data.objects.new(part['name'],mesh);bpy.context.scene.collection.objects.link(obj)
slots=part['material_slots']
for slot,origin in zip(slots,['Eve Legs','Eve Toenails'],strict=True):
    mat=bpy.data.materials.new(data['materials'][slot]);mat['CSS_source_material']=origin
    mesh.materials.append(mat)
for poly,record in zip(mesh.polygons,records,strict=True):
    poly.material_index=slots.index(record[3]);poly.use_smooth=True
for channel in range(data['uv_channels']):
    layer=mesh.uv_layers.new(name=f'UV{channel}')
    for poly,record in zip(mesh.polygons,records,strict=True):
        for loop,corner in zip(poly.loop_indices,record[:3],strict=True):
            u,v=data['wedges'][corner][1+channel*2:3+channel*2]
            layer.data[loop].uv=(u,1-v)
color=mesh.color_attributes.new(name='Color',type='BYTE_COLOR',domain='CORNER')
for poly,record in zip(mesh.polygons,records,strict=True):
    for loop,corner in zip(poly.loop_indices,record[:3],strict=True):
        color.data[loop].color_srgb=[v/255 for v in data['colors'][corner]]
normals=[(data['normals'][i][0],-data['normals'][i][1],data['normals'][i][2]) for f in records for i in f[:3]]
mesh.normals_split_custom_set(normals)
groups={}
expected_weights={}
for v,bone,weight in data['influences']:
    if v<begin:continue
    if bone not in groups:groups[bone]=obj.vertex_groups.new(name=data['bones'][bone]['name'])
    groups[bone].add([v-begin],weight,'REPLACE')
    expected_weights.setdefault(v-begin,{})[data['bones'][bone]['name']]=weight
obj.shape_key_add(name='Basis')
expected_shapes={}
for target in data['morph_targets']:
    deltas={v-begin:converted([x,y,z]) for v,x,y,z in target['deltas'] if v>=begin}
    if not deltas:continue
    key=obj.shape_key_add(name=target['name'])
    for v,delta in deltas.items():key.data[v].co=Vector(positions[v])+Vector(delta)
    expected_shapes[target['name']]=deltas
obj['CSS_export']=True;obj['CSS_source_mesh_sha256']=audit['output_sha256']
obj['CSS_occludes_materials']='SkinCovered_1,SkinCovered_6'
bpy.ops.wm.save_as_mainfile(filepath=str(output),check_existing=False)
bpy.ops.wm.open_mainfile(filepath=str(output))
obj=bpy.data.objects[part['name']];mesh=obj.data
error=max((v.co-Vector(p)).length for v,p in zip(mesh.vertices,positions,strict=True))
assert error<1e-6
assert [list(p.vertices) for p in mesh.polygons]==faces
for v in mesh.vertices:
    actual={obj.vertex_groups[g.group].name:g.weight for g in v.groups}
    assert actual.keys()==expected_weights[v.index].keys()
    assert max(abs(actual[k]-expected_weights[v.index][k]) for k in actual)<1e-6
for name,deltas in expected_shapes.items():
    key=mesh.shape_keys.key_blocks[name]
    for v in range(len(positions)):
        assert (key.data[v].co-Vector(positions[v])-Vector(deltas.get(v,(0,0,0)))).length<1e-6
for channel,layer in enumerate(mesh.uv_layers):
    for poly,record in zip(mesh.polygons,records,strict=True):
        for loop,corner in zip(poly.loop_indices,record[:3],strict=True):
            u,v=data['wedges'][corner][1+channel*2:3+channel*2]
            assert (layer.data[loop].uv-Vector((u,1-v))).length<1e-6
report=dict(source_sha256=audit['output_sha256'],saved_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
    vertices=len(positions),faces=len(faces),reload_position_error_cm=error*100,
    weights_uvs_shapes_verified=True,shape_names=list(expected_shapes),
    scope='Separate lining source library; no rig object. Assembly, normals/material import and game verification remain required.')
(w/'skin-foot-lining.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

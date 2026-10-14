"""Reconstruct an editable part from its accepted import, using the lining recovery workflow."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy
from mathutils import Vector
w=Path(__file__).resolve().parents[2]/'work/eve26'
sys.path.insert(0,str(w.parents[2]/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'))
from export_variant_clean import preserve_corner_normals, export_corner_normals
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--candidate',type=Path,required=True)
parser.add_argument('--part',required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
source=args.candidate;data=json.loads(source.read_text())
audit=json.loads(source.with_suffix('.audit.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest()==audit['output_sha256']
matches=[i for i,p in enumerate(audit['parts']) if p['name']==args.part]
assert len(matches)==1
index=matches[0];part=audit['parts'][index]
begin=sum(p['points'] for p in audit['parts'][:index]);end=begin+part['points']
face_begin=sum(p['faces'] for p in audit['parts'][:index])
records=data['faces'][face_begin:face_begin+part['faces']]
protected={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in
           (source,source.with_suffix('.audit.json'),Path(__file__))}
output=args.output;assert not output.exists() and not output.with_suffix('.json').exists()
bpy.ops.wm.read_factory_settings(use_empty=True)
def converted(p):return (p[0]/100,-p[1]/100,p[2]/100)
positions=[converted(p) for p in data['points'][begin:end]]
faces=[[data['wedges'][i][0]-begin for i in f[:3]] for f in records]
mesh=bpy.data.meshes.new(part['name']);mesh.from_pydata(positions,[],faces)
obj=bpy.data.objects.new(part['name'],mesh);bpy.context.scene.collection.objects.link(obj)
slots=part['material_slots']
for slot in slots:
    origin=data['materials'][slot]
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
# The exporter quantizes scene-linear .color values, not sRGB values.
color=mesh.color_attributes.new(name='Color',type='FLOAT_COLOR',domain='CORNER')
for poly,record in zip(mesh.polygons,records,strict=True):
    for loop,corner in zip(poly.loop_indices,record[:3],strict=True):
        color.data[loop].color=[v/255 for v in data['colors'][corner]]
normals=[(data['normals'][i][0],-data['normals'][i][1],data['normals'][i][2]) for f in records for i in f[:3]]
mesh.normals_split_custom_set(normals)
preserve_corner_normals(obj,normals)
groups={}
expected_weights={}
for v,bone,weight in data['influences']:
    if not begin<=v<end:continue
    if bone not in groups:groups[bone]=obj.vertex_groups.new(name=data['bones'][bone]['name'])
    groups[bone].add([v-begin],weight,'REPLACE')
    expected_weights.setdefault(v-begin,{})[data['bones'][bone]['name']]=weight
obj.shape_key_add(name='Basis').value=0
expected_shapes={}
for target in data['morph_targets']:
    deltas={v-begin:converted([x,y,z]) for v,x,y,z in target['deltas'] if begin<=v<end}
    if not deltas:continue
    key=obj.shape_key_add(name=target['name'])
    key.value=0
    for v,delta in deltas.items():key.data[v].co=Vector(positions[v])+Vector(delta)
    expected_shapes[target['name']]=deltas
obj['CSS_export']=True;obj['CSS_source_mesh_sha256']=audit['output_sha256']
obj['CSS_source_part']=part['name']
obj.show_only_shape_key=False
output.parent.mkdir(parents=True,exist_ok=True)
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
    assert key.value==0
    for v in range(len(positions)):
        assert (key.data[v].co-Vector(positions[v])-Vector(deltas.get(v,(0,0,0)))).length<1e-6
for channel,layer in enumerate(mesh.uv_layers):
    for poly,record in zip(mesh.polygons,records,strict=True):
        for loop,corner in zip(poly.loop_indices,record[:3],strict=True):
            u,v=data['wedges'][corner][1+channel*2:3+channel*2]
            assert (layer.data[loop].uv-Vector((u,1-v))).length<1e-6
color=mesh.color_attributes.active_color
assert color is not None and color.domain=='CORNER'
for poly,record in zip(mesh.polygons,records,strict=True):
    for loop,corner in zip(poly.loop_indices,record[:3],strict=True):
        assert [round(max(0,min(1,v))*255) for v in color.data[loop].color]==data['colors'][corner]
retained=export_corner_normals(obj,mesh)
normal_error=max((entry.vector-Vector(expected)).length for entry,expected in zip(retained,normals,strict=True))
assert normal_error<1e-6
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in protected.items())
report=dict(part=part['name'],inputs=protected,inputs_unchanged=True,source_sha256=audit['output_sha256'],saved_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
    vertices=len(positions),faces=len(faces),reload_position_error_cm=error*100,
    weights_uvs_shapes_verified=True,linear_color_bytes_verified=True,shape_names=list(expected_shapes),
    retained_normal_error=normal_error,
    scope='Import-derived editable part with explicit triangles and guarded float export normals; no rig object. '
          'Blender preview uses its packed normals. Assembly, material import and game verification remain required.')
output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

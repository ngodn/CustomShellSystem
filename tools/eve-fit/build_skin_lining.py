"""Create a reversible Skin Suit footwear lining with an unchanged body seam."""
import argparse,copy,hashlib,json,sys
from collections import defaultdict
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform
import numpy as np

w=Path(__file__).resolve().parents[2]/'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--retain',type=float,default=.01)
parser.add_argument('--output',type=Path,default=w/'skin-lining6')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert 0<args.retain<.1
source=w/'skin-foot-sections/skin.mesh.json'
data=json.loads(source.read_text());audit=json.loads(source.with_suffix('.audit.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest()==audit['output_sha256']
bp_path=w.parents[2]/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work/exports-nextgen/SK_SeduXtress_HandBindV43.mesh.json'
bp=json.loads(bp_path.read_text());bp_audit=json.loads(bp_path.with_suffix('.audit.json').read_text())
assert hashlib.sha256(bp_path.read_bytes()).hexdigest()==bp_audit['output_sha256']
start=0
for part in bp_audit['parts']:
    if part['name']=='Eve Black Pearl - Footwear Lining':break
    start+=part['points']
else:raise ValueError('No accepted footwear lining')
end=start+part['points']
body_uv=defaultdict(set);lining_body=defaultdict(set)
for row in bp['wedges']:
    if row[0]<36787:body_uv[tuple(round(x,6) for x in row[1:])].add(row[0])
for row in bp['wedges']:
    if start<=row[0]<end:lining_body[row[0]].update(body_uv[tuple(round(x,6) for x in row[1:])])
assert all(len(ids)==1 for ids in lining_body.values())
current_uv=defaultdict(set);current_map=defaultdict(set)
for row in data['wedges']:
    if row[0]<36787:current_uv[tuple(round(x,6) for x in row[1:])].add(row[0])
for row in bp['wedges']:
    if start<=row[0]<end:current_map[row[0]].update(current_uv[tuple(round(x,6) for x in row[1:])])
assert all(len(ids)==1 for ids in current_map.values())
targets={next(iter(ids)):Vector(bp['points'][i])-Vector(bp['points'][next(iter(lining_body[i]))]) for i,ids in current_map.items()}
cage_faces=[]
for f in bp['faces']:
    ids=[bp['wedges'][i][0] for i in f[:3]]
    if all(i in current_map for i in ids):cage_faces.append([next(iter(current_map[i])) for i in ids])
cage_points=[Vector(p) for p in data['points'][:36787]]
cage=BVHTree.FromPolygons(cage_points,cage_faces,all_triangles=True)
unmatched=[]
ref=json.loads((w/'skin-ankle-aligned.json').read_text())
skin_tree=BVHTree.FromPolygons([Vector(p) for p in ref['points_cm']],ref['triangles'],all_triangles=True)
covered={20,21}
body_faces=audit['parts'][0]['faces']
selected=[f for f in data['faces'][:body_faces] if f[3] in covered]
used=sorted({data['wedges'][i][0] for f in selected for i in f[:3]})
outside={data['wedges'][i][0] for f in data['faces'][:body_faces] if f[3] not in covered for i in f[:3]}
boundary=set(used)&outside
result=copy.deepcopy(data)
point_map={old:len(data['points'])+i for i,old in enumerate(used)}
distances=[]
for old in used:
    point=Vector(data['points'][old])
    if old not in targets:
        hit,_,face,distance=cage.find_nearest(point)
        assert distance<3,(old,distance)
        a,b,c=cage_faces[face]
        weights=barycentric_transform(hit,cage_points[a],cage_points[b],cage_points[c],Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
        targets[old]=sum((targets[i]*weight for i,weight in zip((a,b,c),weights)),Vector())
        unmatched.append([old,distance])
    hit=point+targets[old]
    surface,_,_,distance=skin_tree.find_nearest(hit)
    assert distance<6,(old,distance)
    # Keep a small amount of the heel volume rather than flattening it entirely.
    hit=hit.lerp(surface,.95)
    t=0 if old in boundary else max(0,min(1,(22-point.z)/5))
    t=t*t*(3-2*t)
    target=point.lerp(hit,t*(1-args.retain))
    assert (target-point).length<20
    distances.append((target-point).length)
    result['points'].append(list(target))
for old in boundary:assert result['points'][point_map[old]]==data['points'][old]
precision_repairs={}
for iteration in range(20):
    failures=0
    for face in selected:
        original=[data['wedges'][i][0] for i in face[:3]]
        indices=[point_map[i] for i in original]
        tri=np.asarray([result['points'][i] for i in indices],dtype=np.float32)
        cross=np.cross(tri[1]-tri[0],tri[2]-tri[0])
        if float(cross@cross)>=1e-12:continue
        failures+=1
        movable=[k for k in range(3) if original[k] not in boundary]
        assert movable,'Importer-small triangle touches only protected seam vertices'
        k=max(movable,key=lambda k:np.linalg.norm(tri[(k+2)%3]-tri[(k+1)%3]))
        edge=Vector(tri[(k+2)%3].tolist())-Vector(tri[(k+1)%3].tolist())
        normal=Vector(data['normals'][face[0]])
        direction=normal.cross(edge).normalized()
        assert direction.length>.99 and edge.length>1e-5
        step=direction*(1e-5/edge.length)
        assert step.length<.005,'Precision repair exceeds 0.05 mm'
        idx=indices[k];precision_repairs.setdefault(idx,Vector(result['points'][idx]))
        value=Vector(result['points'][idx])+step
        assert (value-precision_repairs[idx]).length<.005
        result['points'][idx]=list(value)
    if failures==0:break
assert failures==0,'Importer precision repair did not converge'
material_map={20:len(data['materials']),21:len(data['materials'])+1}
result['materials']+=['SkinFootLining','SkinToenailLining']
wedges={}
for face in selected:
    new=[]
    for old in face[:3]:
        if old not in wedges:
            wedges[old]=len(result['wedges'])
            record=copy.deepcopy(data['wedges'][old]);record[0]=point_map[record[0]]
            result['wedges'].append(record)
            result['normals'].append(copy.deepcopy(data['normals'][old]))
            result['colors'].append(copy.deepcopy(data['colors'][old]))
        new.append(wedges[old])
    result['faces'].append(new+[material_map[face[3]]])
result['influences'] += [[point_map[v],bone,weight] for v,bone,weight in data['influences'] if v in point_map]
for target,old in zip(result['morph_targets'],data['morph_targets']):
    target['deltas'] += [[point_map[v],x,y,z] for v,x,y,z in old['deltas'] if v in point_map]
# Recalculate only the new lining normals; original exported corners stay intact.
normals={v:Vector() for v in point_map.values()}
for f in result['faces'][len(data['faces']):]:
    indices=[result['wedges'][i][0] for i in f[:3]]
    a,b,c=[Vector(result['points'][i]) for i in indices]
    n=-(b-a).cross(c-a)
    for i in indices:normals[i]+=n
for old,new in wedges.items():
    n=normals[result['wedges'][new][0]]
    vertex=data['wedges'][old][0]
    blend=0 if vertex in boundary else max(0,min(1,(22-data['points'][vertex][2])/5))
    blend=blend*blend*(3-2*blend)
    original=Vector(data['normals'][old])
    result['normals'][new]=list(original.lerp(n.normalized(),blend).normalized()) if n.length>1e-9 else list(original)
assert result['points'][:len(data['points'])]==data['points']
assert result['faces'][:len(data['faces'])]==data['faces']
result['mesh_package']='/Game/CSS/EveTest/SK_SkinLining'
out=args.output;out.mkdir(exist_ok=False)
path=out/'skin.mesh.json';path.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
audit['parts'].append(dict(name='Eve Skin Suit - Footwear Lining',points=len(used),faces=len(selected),material_slots=list(material_map.values())))
audit['mesh_package']=result['mesh_package'];audit['stage']='Private ankle lining candidate; requires deformation and material validation'
(out/'skin.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
controls=json.loads((w/'skin-foot-sections/controls.json').read_text())
controls[0]['sections']+=list(material_map.values())
(out/'controls.json').write_text(json.dumps(controls,indent=2)+'\n')
(out/'receipt.json').write_text(json.dumps(dict(points=len(used),faces=len(selected),boundary_vertices=len(boundary),
    max_displacement_cm=max(distances),interpolated_vertices=unmatched,original_geometry_unchanged=True,body_seam_exact=True,
    original_volume_retained=args.retain,seam_normal_blend_cm=[17,22],
    precision_repaired_vertices=len(precision_repairs),precision_max_offset_cm=max(((Vector(result['points'][i])-v).length for i,v in precision_repairs.items()),default=0),
    scope='Duplicate foot surface only. Inherited morphs and weights need motion checks; body UVs retained. Not runtime acceptance.'),indent=2)+'\n')

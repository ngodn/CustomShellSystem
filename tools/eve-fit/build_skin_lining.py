"""Create a reversible Skin Suit footwear lining with an unchanged body seam."""
import copy,hashlib,json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree

w=Path(__file__).resolve().parents[2]/'work/eve26'
source=w/'skin-foot-sections/skin.mesh.json'
data=json.loads(source.read_text());audit=json.loads(source.with_suffix('.audit.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest()==audit['output_sha256']
ref=json.loads((w/'skin-ankle-aligned.json').read_text())
tree=BVHTree.FromPolygons([Vector(p) for p in ref['points_cm']],ref['triangles'],all_triangles=True)
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
    hit,_,_,distance=tree.find_nearest(point)
    t=0 if old in boundary else max(0,min(1,(22-point.z)/5))
    t=t*t*(3-2*t)
    target=point.lerp(hit,t)
    assert (target-point).length<20
    distances.append((target-point).length)
    result['points'].append(list(target))
for old in boundary:assert result['points'][point_map[old]]==data['points'][old]
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
    result['normals'][new]=list(n.normalized()) if n.length>1e-9 else data['normals'][old]
assert result['points'][:len(data['points'])]==data['points']
assert result['faces'][:len(data['faces'])]==data['faces']
result['mesh_package']='/Game/CSS/EveTest/SK_SkinLining'
out=w/'skin-lining1';out.mkdir(exist_ok=False)
path=out/'skin.mesh.json';path.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
audit['parts'].append(dict(name='Eve Skin Suit - Footwear Lining',points=len(used),faces=len(selected),material_slots=list(material_map.values())))
audit['mesh_package']=result['mesh_package'];audit['stage']='Private ankle lining candidate; requires deformation and material validation'
(out/'skin.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
controls=json.loads((w/'skin-foot-sections/controls.json').read_text())
controls[0]['sections']+=list(material_map.values())
(out/'controls.json').write_text(json.dumps(controls,indent=2)+'\n')
(out/'receipt.json').write_text(json.dumps(dict(points=len(used),faces=len(selected),boundary_vertices=len(boundary),
    max_displacement_cm=max(distances),original_geometry_unchanged=True,body_seam_exact=True,
    scope='Duplicate foot surface only. Inherited morphs and weights need motion checks; body UVs retained. Not runtime acceptance.'),indent=2)+'\n')

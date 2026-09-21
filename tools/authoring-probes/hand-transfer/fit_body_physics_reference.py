"""Fit an isolated game-derived collider template to verified B2 skin, Blender 5.2.2.

Coverage is assigned neutral skin vertices, not game contact or collision acceptance.
"""
import copy,hashlib,json,math,os,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix,Quaternion,Vector
ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=Path(os.environ['CSS_BODY_PHYSICS_FIT_DIR']).resolve()
assert OUT.parent==WORK.resolve() and not OUT.exists();OUT.mkdir()
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0,str(MOD/'tools'))
from export_seduxtress_eve import read_bones,TO_UE
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source_path=WORK/'arm-rest-correctives-export-v1/candidate.mesh.json'
template_path=WORK/'b2-body-physics-reference-v1/template.json'
reference_path=WORK/'morebeaute-reference-v1/complete/35488a0c738dc559bdaea55998de0c386d624cd105997.refskel.json'
bind_path=WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json'
hashes={str(p):digest(p) for p in (source_path,template_path,reference_path,bind_path)}
source=load(source_path);template=load(template_path);candidate=copy.deepcopy(template)
bones,world=read_bones(bind_path);old_bones,old_world=read_bones(reference_path)
by_name={b['name'].lower():i for i,b in enumerate(bones)}
old_by_name={b['name'].lower():i for i,b in enumerate(old_bones)}
core={b['BoneName'].lower() for b in template['bodies']};assert len(core)==22
points=np.asarray(source['points'],dtype=float)
# Skin includes the main body and both modular foot skin/nail variants.
# The neutral collider envelope covers their union; garments and hair are excluded.
assert source['materials'][:8]==['Eve Face','Eve Head','Eve Body','Eve Legs','Eve Arms','Eve Genitals','Eve Fingernails','Eve Toenails']
skin_slots=set(range(8))|{23,24,25,26}
assert source['materials'][23:27]==['Eve Covered Feet','Eve Covered Toenails','Eve Footwear Skin','Eve Footwear Toenails']
triangles=[[source['wedges'][w][0] for w in f[:3]] for f in source['faces'] if f[3] in skin_slots]
body_indices=sorted({i for f in triangles for i in f});body_set=set(body_indices)
# This fixture contains 19,105 unique skin/nail points, not the garment-inclusive
# point count. Pin the export so a changed topology requires a fresh audit.
assert digest(source_path)=='f49fd4e69a554f99bb2c309c78688192bd839285db178a9c83e013181b640d8a'
assert len({source['wedges'][w][0] for f in source['faces'] if f[3]<8 for w in f[:3]})==19105
assert all(any(f[3]==slot for f in source['faces']) for slot in (23,24,25,26))
assert [sum(f[3]==i for f in source['faces']) for i in range(8)]==[17538,252,4708,2192,7772,4462,1120,0]
ancestor=[]
for i,b in enumerate(bones):
    name=b['name'].lower();ancestor.append(name if name in core else ancestor[b['parent']] if b['parent']>=0 else None)
weights={i:{} for i in body_indices}
for vertex,bone,weight in source['influences']:
    if vertex not in body_set:continue
    name=ancestor[bone]
    if name:weights[vertex][name]=weights[vertex].get(name,0.)+weight
regions={n:[] for n in core}
for vertex,row in weights.items():
    assert row,(vertex,'no human body ancestor')
    regions[max(row,key=row.get)].append(vertex)
(OUT/'region-counts.json').write_text(json.dumps({n:len(ids) for n,ids in sorted(regions.items())},indent=2)+'\n')
assert all(len(ids)>=8 for ids in regions.values()),{n:len(ids) for n,ids in regions.items() if len(ids)<8}
def rotator(value):
    # Pinned UE5.6.1 FRotator3d::Quaternion scalar formula (UnrealMath.cpp).
    p,y,r=[math.radians(value.get(k,0))/2 for k in ('Pitch','Yaw','Roll')]
    sp,sy,sr=math.sin(p),math.sin(y),math.sin(r);cp,cy,cr=math.cos(p),math.cos(y),math.cos(r)
    return Quaternion((cr*cp*cy+sr*sp*sy,cr*sp*sy-sr*cp*cy,-cr*sp*cy-sr*cp*sy,cr*cp*sy-sr*sp*cy)).normalized()
def xyz(value):return np.asarray([value.get(k,0.) for k in 'XYZ'])
def vector(value):return dict(zip('XYZ',map(float,value)))
def local_points(name,ids):
    inverse=np.asarray(world[by_name[name]].inverted(),dtype=float)
    return points[ids]@inverse[:3,:3].T+inverse[:3,3]
def sdf(shape,kind,cloud):
    r=np.asarray(rotator(shape['Rotation']).to_matrix(),dtype=float)
    p=(cloud-xyz(shape['Center']))@r
    if kind=='BoxElems':
        q=np.abs(p)-np.asarray([shape[k] for k in 'XYZ'])/2
        return np.linalg.norm(np.maximum(q,0),axis=1)+np.minimum(np.max(q,axis=1),0)
    radius=shape.get('Radius',shape.get('Radius0'));assert radius==shape.get('Radius1',radius)
    q=np.column_stack((p[:,:2],np.maximum(np.abs(p[:,2])-shape['Length']/2,0)))
    return np.linalg.norm(q,axis=1)-radius
margin=.35;reports=[]
for body,previous in zip(candidate['bodies'],template['bodies'],strict=True):
    name=body['BoneName'].lower();cloud=local_points(name,regions[name])
    originals=[(kind,s) for kind,rows in previous['AggGeom'].items() for s in rows]
    targets=[(kind,s) for kind,rows in body['AggGeom'].items() for s in rows]
    assignment=np.argmin(np.stack([sdf(s,k,cloud) for k,s in originals]),axis=0)
    for index,((kind,shape),(_,old)) in enumerate(zip(targets,originals,strict=True)):
        selected=cloud[assignment==index];assert len(selected)>=8,(name,kind,len(selected))
        rotation=np.asarray(rotator(shape['Rotation']).to_matrix(),dtype=float)
        p=selected@rotation;center=(p.min(axis=0)+p.max(axis=0))/2;q=p-center
        shape['Center']=vector(rotation@center)
        if kind=='BoxElems':
            for axis,value in zip('XYZ',p.max(axis=0)-p.min(axis=0)+2*margin):shape[axis]=float(value)
        else:
            radial=np.linalg.norm(q[:,:2],axis=1);minimum=float(radial.max()+margin)
            maximum=max(minimum,float(np.linalg.norm(q,axis=1).max()+margin))
            choices=[]
            for radius in np.linspace(minimum,maximum,33):
                half=max(0.,float(np.max(np.abs(q[:,2])-np.sqrt(np.maximum(0,radius*radius-radial*radial)))))
                half+=margin
                volume=math.pi*radius*radius*(2*half)+4/3*math.pi*radius**3
                choices.append((volume,float(radius),half))
            _,radius,half=min(choices)
            shape['Length']=2*half
            if kind=='SphylElems':shape['Radius']=radius
            else:shape['Radius0']=shape['Radius1']=radius
        old_distance=sdf(old,kind,selected);new_distance=sdf(shape,kind,selected)
        assert np.max(new_distance)<.0001,(name,kind,float(np.max(new_distance)))
        reports.append(dict(bone=name,kind=kind,vertices=len(selected),old_outside_vertices=int(np.sum(old_distance>.001)),
            old_maximum_outside_cm=max(0.,float(old_distance.max())),new_maximum_outside_cm=max(0.,float(new_distance.max())),
            old=old,new=shape))
# Preserve each source joint's relative rest-frame rotation. Move the second
# anchor to the first anchor on the target anatomy, without altering limits/drives.
constraint_checks=[]
def axis(instance,key,default):return Vector([instance.get(key,dict(zip('XYZ',default)))[a] for a in 'XYZ'])
def frame(rotation,primary,secondary):return Matrix((rotation@primary,rotation@secondary,(rotation@primary).cross(rotation@secondary))).transposed()
for constraint,original in zip(candidate['constraints'],template['constraints'],strict=True):
    value=constraint['DefaultInstance'];old=original['DefaultInstance'];a,b=[value[k].lower() for k in ('ConstraintBone1','ConstraintBone2')]
    wa,wb=world[by_name[a]],world[by_name[b]];sa,sb=old_world[old_by_name[a]],old_world[old_by_name[b]]
    pos1=axis(old,'Pos1',(0,0,0));anchor=wa@pos1;value['Pos2']=vector(wb.inverted()@anchor)
    p1,s1=axis(old,'PriAxis1',(1,0,0)),axis(old,'SecAxis1',(0,1,0))
    p2,s2=axis(old,'PriAxis2',(1,0,0)),axis(old,'SecAxis2',(0,1,0))
    relative=frame(sa.to_3x3(),p1,s1).inverted()@frame(sb.to_3x3(),p2,s2)
    desired=frame(wa.to_3x3(),p1,s1)@relative;local=wb.to_3x3().inverted()@desired
    value['PriAxis2']=vector(local.col[0]);value['SecAxis2']=vector(local.col[1])
    error=(wb@Vector(list(value['Pos2'].values()))-anchor).length
    assert error<.001,(a,b,error)
    assert {k:v for k,v in value.items() if k not in ('Pos2','PriAxis2','SecAxis2')}=={k:v for k,v in old.items() if k not in ('Pos2','PriAxis2','SecAxis2')}
    constraint_checks.append(dict(body1=a,body2=b,anchor_error_cm=error))
# Engine query fixtures use each primitive center along its three own axes,
# plus six regional skin extrema. Off-body controls are translated far away.
queries=[]
for body in candidate['bodies']:
    name=body['BoneName'].lower();matrix=world[by_name[name]]
    for kind,shapes in body['AggGeom'].items():
        for index,shape in enumerate(shapes):
            center=Vector(xyz(shape['Center']));rotation=rotator(shape['Rotation'])
            for axis in range(3):
                direction=rotation@Vector([100. if a==axis else 0. for a in range(3)])
                start=matrix@(center-direction);end=matrix@(center+direction)
                key=f'{name}/{kind}/{index}/{axis}'
                queries.append(dict(id=key,start=list(start),end=list(end),expected=True))
                offset=Vector((1000,1000,1000))
                queries.append(dict(id=key+'/miss',start=list(start+offset),end=list(end+offset),expected=False))
    cloud=points[regions[name]]
    for axis in range(3):
        for label,selection in [('min',np.argmin),('max',np.argmax)]:
            point=Vector(cloud[int(selection(cloud[:,axis]))]);direction=Vector((0,100,0))
            queries.append(dict(id=f'{name}/skin/{axis}/{label}',start=list(point-direction),end=list(point+direction),expected=True))
(OUT/'queries.json').write_text(json.dumps(queries,indent=2)+'\n')
candidate.update(fit_verified=False,neutral_vertex_fit=True,scope=__doc__)
(OUT/'candidate.json').write_text(json.dumps(candidate,indent=2)+'\n')
(OUT/'report.json').write_text(json.dumps(dict(passed=True,body_vertices=len(body_indices),margin_cm=margin,shapes=reports,constraints=constraint_checks,
    protected_hashes=hashes,scope=__doc__,visual_review_pending=True,physics_queries_verified=False),indent=2)+'\n')
assert all(digest(Path(p))==h for p,h in hashes.items())
print(json.dumps(dict(shapes=len(reports),constraints=len(constraint_checks),body_vertices=len(body_indices),old_outside=sum(r['old_outside_vertices'] for r in reports))),flush=True)

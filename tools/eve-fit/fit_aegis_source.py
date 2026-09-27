"""Transfer the original War Aegis surface offsets onto the unchanged CSS body."""
import copy
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import fitted_points, frame

root=Path(__file__).resolve().parents[3]
mod=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
w=root/'CustomShellSystem/work/eve26'
source=mod/'reference/body-type-variant-EVE/eve_beta10.blend'
export=mod/'gemini-work/exports/SK_Eve_WarAegis.mesh.json'
out=w/'aegis-fit1'
assert not out.exists()
source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
data=json.loads(export.read_text());audit=json.loads(export.with_suffix('.audit.json').read_text())
assert hashlib.sha256(export.read_bytes()).hexdigest()==audit['output_sha256']
nb=audit['parts'][0]['points'];ng=audit['parts'][1]['points']
bpy.ops.wm.open_mainfile(filepath=str(source))
body=bpy.data.objects['Eve Body'];garment=bpy.data.objects['Eve War Aegis - Suit']
source_body=fitted_points(body);source_cloth=fitted_points(garment)
# Use the exporter's nondegenerate vertex ordering, checking topology before transfer.
def topology(obj,points):
    obj.data.calc_loop_triangles()
    triangles=[tuple(t.vertices) for t in obj.data.loop_triangles if (points[t.vertices[1]]-points[t.vertices[0]]).cross(points[t.vertices[2]]-points[t.vertices[0]]).length_squared>=1e-20]
    used=sorted({i for t in triangles for i in t});index={v:i for i,v in enumerate(used)}
    return used,[tuple(index[v] for v in t) for t in triangles]
bused,bfaces=topology(body,source_body);gused,gfaces=topology(garment,source_cloth)
assert len(bused)==nb and len(gused)==ng,(len(bused),nb,len(gused),ng)
for triangles,first,count,start in [(bfaces,0,audit['parts'][0]['faces'],0),(gfaces,audit['parts'][0]['faces'],audit['parts'][1]['faces'],nb)]:
    expected=[tuple(sorted(data['wedges'][i][0]-start for i in f[:3])) for f in data['faces'][first:first+count]]
    obj,used = (body,bused) if start == 0 else (garment,gused)
    polygons=[set(p.vertices) for p in obj.data.polygons]
    membership=[set() for _ in obj.data.vertices]
    for pi,vertices in enumerate(polygons):
        for vi in vertices:membership[vi].add(pi)
    assert all(set.intersection(*(membership[used[v]] for v in t)) for t in expected),'Export triangle crosses source polygon'
    # Blender may choose another diagonal after fitting; keep the export topology.
    if start == 0:bfaces=[tuple(t) for t in expected]
src=[source_body[i] for i in bused]
dst=[Vector((p[0]/100,-p[1]/100,p[2]/100)) for p in data['points'][:nb]]
tree=BVHTree.FromPolygons(src,bfaces,all_triangles=True)
result=copy.deepcopy(data);correspondence=[];distances=[]
weights=[{} for _ in range(nb)]
for i,b,v in data['influences']:
    if i<nb:weights[i][b]=v
new_weights=[]
for j,i in enumerate(gused):
    point=source_cloth[i];hit,normal,ti,distance=tree.find_nearest(point)
    ids=bfaces[ti];a=[src[k] for k in ids];b=[dst[k] for k in ids]
    rotation=frame(*b)@frame(*a).transposed()
    fitted=barycentric_transform(hit,*a,*b)+rotation@(point-hit)
    result['points'][nb+j]=[fitted.x*100,-fitted.y*100,fitted.z*100]
    bary=barycentric_transform(hit,*a,Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
    alpha=np.maximum(list(bary),0);alpha/=alpha.sum();correspondence.append((ids,alpha))
    combined={}
    for vi,factor in zip(ids,alpha):
        for bone,weight in weights[vi].items():combined[bone]=combined.get(bone,0)+weight*float(factor)
    kept=sorted(((b,v) for b,v in combined.items() if v>1e-7),key=lambda item:-item[1])[:8]
    total=sum(v for _,v in kept);assert total>0
    new_weights.extend([nb+j,b,v/total] for b,v in kept)
    distances.append(distance*100)
result['influences']=[r for r in data['influences'] if not nb<=r[0]<nb+ng]+new_weights
for target in result['morph_targets']:
    if target['name'] not in ('FBMBodyTone','PBMBreastsSize','PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth'):continue
    body_delta=np.zeros((nb,3))
    for i,*delta in target['deltas']:
        if i<nb:body_delta[i]=delta
    replacement=[]
    for j,(ids,alpha) in enumerate(correspondence):
        delta=sum((body_delta[i]*v for i,v in zip(ids,alpha)),np.zeros(3))
        if np.linalg.norm(delta)>1e-5:replacement.append([nb+j,*delta.tolist()])
    target['deltas']=[d for d in target['deltas'] if not nb<=d[0]<nb+ng]+replacement
assert result['points'][:nb]==data['points'][:nb] and result['points'][nb+ng:]==data['points'][nb+ng:]
assert result['bones']==data['bones'] and result['faces']==data['faces'] and result['wedges']==data['wedges']
assert hashlib.sha256(source.read_bytes()).hexdigest()==source_hash
out.mkdir();path=out/'aegis.mesh.json';path.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
(out/'aegis.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'receipt.json').write_text(json.dumps(dict(source_sha256=source_hash,source_unchanged=True,body_geometry_unchanged=True,skeleton_unchanged=True,garment_points=ng,source_surface_distance_max_cm=max(distances),scope='Original saved fit transferred by body triangle correspondence. Garment geometry, weights and six shape deltas changed; visual and gameplay validation required; normals need refresh before production.'),indent=2)+'\n')
print(out,flush=True)

"""Fit a private sphere recipe to sampled motion while preserving the render assets."""
import hashlib
import json
import sys
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2]/'work/eve26'
coverage_only = '--coverage-only' in sys.argv
surface_seeds = '--surface-seeds' in sys.argv
output = work/('knit-w2/coverage-regions.json' if coverage_only else 'knit-spheres6.json' if surface_seeds else 'knit-spheres4.json')
assert not output.exists()
path = work/'knit-w2/knit.mesh.json'
mesh = json.loads(path.read_text())
audit = json.loads(path.with_suffix('.audit.json').read_text())
old = json.loads((work/'knit-spheres2.json').read_text())
parents = json.loads((work/'knit-w2/collider-parents.json').read_text())['candidates']
motion = json.loads((work/'knit-cloth2/sprint.json').read_text())
proxy = json.loads((work/'knit-w2/proxy.json').read_text())['slots']['Collar-1']
count = audit['parts'][0]['points']
points = np.asarray(mesh['points'])
body_faces = [[mesh['wedges'][i][0] for i in f[:3]] for f in mesh['faces'][:audit['parts'][0]['faces']]]
garment_faces = [[mesh['wedges'][i][0] for i in f[:3]] for f in mesh['faces'][audit['parts'][0]['faces']:audit['parts'][0]['faces']+audit['parts'][1]['faces']]]
garment = BVHTree.FromPolygons(points.tolist(),garment_faces,all_triangles=True)
region_ids = [i for i in sorted({v for f in body_faces for v in f}) if garment.find_nearest(Vector(points[i]))[3]<1]
proxy_tree = BVHTree.FromPolygons(proxy['positions'],np.asarray(proxy['indices']).reshape(-1,3).tolist(),all_triangles=True)
lookup = {b['name']:i for i,b in enumerate(mesh['bones'])}
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    bind.append(bind[bone['parent']] @ local if bone['parent']>=0 else local)
weights = np.asarray([r for r in mesh['influences'] if r[0]<count])
vi,bi,wt = weights[:,0].astype(int),weights[:,1].astype(int),weights[:,2]
frames = [-1,0,8,16,24,32,40,48,56,64]
poses, trees, regions = [], [], []
for frame in frames:
    pose = bind
    if frame>=0:
        snap=motion['frames'][frame]['pose']['Snapshot'];entries=dict(zip(snap['BoneNames'],snap['LocalTransforms'],strict=True));pose=[]
        for bone in mesh['bones']:
            t=entries[bone['name']]
            local=Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
            pose.append(pose[bone['parent']] @ local if bone['parent']>=0 else local)
    matrices=np.asarray([np.asarray(a @ b.inverted()) for a,b in zip(pose,bind,strict=True)])
    body=np.zeros((count,3))
    np.add.at(body,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],points[vi])+matrices[bi,:3,3])*wt[:,None])
    poses.append(pose);trees.append(BVHTree.FromPolygons(body.tolist(),body_faces,all_triangles=True));regions.append(body[region_ids])
if coverage_only:
    recipe_path = work/'knit-spheres4.json'
    recipe = json.loads(recipe_path.read_text())
    assert recipe['source_sha256'] == hashlib.sha256(path.read_bytes()).hexdigest()
    regions = np.asarray(regions)
    distances = []
    for sphere in recipe['spheres']:
        moved = np.asarray([pose[lookup[sphere['bone']]] @ Vector(sphere['local_center_cm']) for pose in poses])
        distances.append(np.maximum(np.linalg.norm(regions-moved[:,None,:],axis=2)-sphere['radius_cm'],0))
    gaps = np.min(distances,axis=0)
    assert abs(float(gaps.max())-recipe['candidate_unsigned_gaps_cm']['maximum']) < 1e-4
    dominant = {}
    for vertex,bone,weight in weights:
        vertex = int(vertex)
        if vertex not in dominant or weight > dominant[vertex][1]:
            dominant[vertex] = (mesh['bones'][int(bone)]['name'],float(weight))
    rows = []
    for i,vertex in enumerate(region_ids):
        frame_index = int(np.argmax(gaps[:,i]))
        rows.append(dict(vertex=vertex,bone=dominant[vertex][0],rest_cm=points[vertex].tolist(),
            worst_frame=frames[frame_index],posed_cm=regions[frame_index,i].tolist(),
            maximum_gap_cm=float(gaps[frame_index,i])))
    groups = []
    for name in sorted({r['bone'] for r in rows}):
        group = [r for r in rows if r['bone']==name]
        groups.append(dict(bone=name,vertices=len(group),over_2cm=sum(r['maximum_gap_cm']>2 for r in group),
            maximum_gap_cm=max(r['maximum_gap_cm'] for r in group)))
    groups.sort(key=lambda r:r['maximum_gap_cm'],reverse=True)
    output.write_text(json.dumps(dict(recipe=str(recipe_path.relative_to(work)),regions=groups,
        vertices=sorted(rows,key=lambda r:r['maximum_gap_cm'],reverse=True),
        scope='Unsigned coverage for the existing 96-sphere candidate, ten default-morph samples. Body vertex groups identify where new centers are needed; this is not cloth simulation.'),indent=2)+'\n')
    print(json.dumps(groups,indent=2),flush=True)
    raise SystemExit(0)
def inside(tree,center):
    for direction in (Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1))):
        origin=center.copy();hits=0
        for _ in range(100):
            hit,_,_,_=tree.ray_cast(origin,direction,400)
            if hit is None:break
            hits+=1;origin=hit+direction*.0001
        else:raise AssertionError('Ray limit')
        if hits%2!=1:return False
    return True
candidates=[]
seeds = list(old['spheres'])
if surface_seeds:
    coverage = json.loads((work/'knit-w2/coverage-regions.json').read_text())
    normals = np.zeros_like(points)
    triangles = np.asarray(body_faces)
    face_normals = np.cross(points[triangles[:,1]]-points[triangles[:,0]],points[triangles[:,2]]-points[triangles[:,0]])
    for corner in range(3):
        np.add.at(normals,triangles[:,corner],face_normals)
    selected_points = []
    for row in coverage['vertices']:
        if row['maximum_gap_cm'] < 2: break
        vertex = row['vertex']
        point = points[vertex]
        if any(np.linalg.norm(point-p)<2 for p in selected_points):continue
        normal = normals[vertex]
        if np.linalg.norm(normal)<1e-8:continue
        normal = normal/np.linalg.norm(normal)
        # The exported winding is not guaranteed to point out of the body.
        if not inside(trees[0],Vector(point-normal*.25)):
            normal = -normal
        if not inside(trees[0],Vector(point-normal*.25)):continue
        selected_points.append(point)
        for depth in (1.,2.,3.):
            center = point-normal*depth
            if not inside(trees[0],Vector(center)):continue
            seeds.append(dict(center_cm=center.tolist(),radius_cm=depth,source_vertex=vertex))
    print('surface sites',len(selected_points),'total seeds',len(seeds),flush=True)
for index,sphere in enumerate(seeds):
    names={p['bone'] for p in parents if p['sphere']==index}
    if index>=len(old['spheres']):
        names.update(mesh['bones'][int(b)]['name'] for v,b,w in weights if int(v)==sphere['source_vertex'] and w>.01)
    for name in list(names):
        parent=mesh['bones'][lookup[name]]['parent']
        if parent>=0 and mesh['bones'][parent]['name']!='root':names.add(mesh['bones'][parent]['name'])
    step=min(2.,sphere['radius_cm']*.35)
    shifts=[np.zeros(3)]
    if index<len(old['spheres']):
        shifts += [np.eye(3)[axis]*sign*step for axis in range(3) for sign in (-1,1)]
    for shift in shifts:
        center=Vector(np.asarray(sphere['center_cm'])+shift)
        clearance=proxy_tree.find_nearest(center)[3]-.15
        for name in sorted(names):
            local=bind[lookup[name]].inverted() @ center
            moved=[pose[lookup[name]] @ local for pose in poses]
            radius=min(clearance,*(tree.find_nearest(c)[3]-.15 for tree,c in zip(trees,moved,strict=True)))
            if radius<.5 or not all(inside(tree,c) for tree,c in zip(trees,moved,strict=True)):continue
            candidates.append(dict(bone=name,center_cm=list(center),local_center_cm=list(local),radius_cm=radius,
                source_vertex=sphere['source_vertex'],source_sphere=index,moved=np.asarray(moved)))
    print('seed',index,'candidates',len(candidates),flush=True)
assert candidates
regions=np.asarray(regions)
gaps=np.stack([np.maximum(np.linalg.norm(regions-c['moved'][:,None,:],axis=2)-c['radius_cm'],0).reshape(-1) for c in candidates]).astype(np.float32)
def stats(values):
    return dict(median=float(np.median(values)),p95=float(np.percentile(values,95)),maximum=float(values.max()))
current=np.full(gaps.shape[1],30.,dtype=np.float32);chosen=[];budgets=[]
for _ in range(min(96,len(candidates))):
    gains=np.maximum(current[None,:]-gaps,0).sum(axis=1);gains[chosen]=-1
    selected=int(np.argmax(gains))
    if gains[selected]<1e-6:break
    chosen.append(selected);current=np.minimum(current,gaps[selected])
    if len(chosen) in (16,32,64,96):budgets.append(dict(spheres=len(chosen),gaps_cm=stats(current)))
baseline=np.full_like(current,30.)
for sphere in old['spheres']:
    moved=np.asarray([pose[lookup[sphere['bone']]] @ Vector(sphere['local_center_cm']) for pose in poses])
    gap=np.maximum(np.linalg.norm(regions-moved[:,None,:],axis=2)-sphere['radius_cm'],0).reshape(-1)
    baseline=np.minimum(baseline,gap)
spheres=[{k:v for k,v in candidates[i].items() if k!='moved'} for i in chosen]
report=dict(source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),spheres=spheres,connections=[],
    frames=frames,candidate_count=len(candidates),sphere_count=len(spheres),body_region_vertices=len(region_ids),seed_count=len(seeds),
    original_unsigned_gaps_cm=stats(baseline),candidate_unsigned_gaps_cm=stats(current),
    budget_comparison=budgets,all_candidates_lower_bound_cm=stats(gaps.min(axis=0)),
    scope='Sampled default-morph inscribed spheres with 0.15 cm inset and rest proxy triangle clearance. Unsigned coverage rewards the old protruding spheres, so compare alongside protrusion. No interpolation, max morphs, native simulation or game acceptance.')
output.write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k!='spheres'},flush=True)

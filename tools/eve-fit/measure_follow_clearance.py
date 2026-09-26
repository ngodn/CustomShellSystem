"""Measure lower-dress/body proximity on sampled actual post-process poses."""
import json
import argparse
import sys
from pathlib import Path
import numpy as np
from mathutils import Matrix,Quaternion,Vector
from mathutils.bvhtree import BVHTree

work=Path(__file__).resolve().parents[2]/'work/eve26'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--clip',choices=('walk','jog','sprint'))
p.add_argument('--frame',type=int)
p.add_argument('--rays',action='store_true')
p.add_argument('--min-z',type=float,default=float('-inf'))
p.add_argument('--max-z',type=float,default=120.)
p.add_argument('--exclude-arms',action='store_true',help='Separate swinging-arm contact from torso/leg fitting')
p.add_argument('--output',type=Path,default=work/'follow-clearance.json')
args=p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
output=args.output;assert not output.exists()
mesh=json.loads((work/'holiday-hip-clean.mesh.json').read_text())
audit=json.loads((work/'holiday.mesh.audit.json').read_text())
base=np.asarray(mesh['points'],dtype=float)
arm_mass={}
for v,b,w in mesh['influences']:
    name=mesh['bones'][b]['name']
    if any(part in name for part in ('upperarm','lowerarm','hand','thumb','index','middle','ring','pinky')):
        arm_mass[v]=arm_mass.get(v,0.)+w
body_count=audit['parts'][0]['faces']
body_faces=[]
for face in mesh['faces'][:body_count]:
    ids=[mesh['wedges'][w][0] for w in face[:3]]
    if args.exclude_arms and all(arm_mass.get(v,0.)>.5 for v in ids):
        continue
    normal=np.sum([mesh['normals'][w] for w in face[:3]],axis=0)
    if np.dot(np.cross(base[ids[1]]-base[ids[0]],base[ids[2]]-base[ids[0]]),normal)<0:
        ids[1],ids[2]=ids[2],ids[1]
    body_faces.append(ids)
slot=mesh['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
vertices=sorted({mesh['wedges'][w][0] for f in mesh['faces'] if f[3]==slot for w in f[:3]
                 if args.min_z<=mesh['points'][mesh['wedges'][w][0]][2]<args.max_z})
assert vertices,'No garment vertices in the requested rest-height interval'
influences={}
for v,b,w in mesh['influences']:
    influences.setdefault(v,[]).append({'bone':mesh['bones'][b]['name'],'weight':w})
rows=np.asarray(mesh['influences'])
vi=rows[:,0].astype(int);bi=rows[:,1].astype(int);weights=rows[:,2,None]
bind=[]
for bone in mesh['bones']:
    q=bone['rotation']
    m=Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    if bone['parent']>=0:m=bind[bone['parent']]@m
    bind.append(m)
inverse=[m.inverted() for m in bind]
cases={}
for label,settings in [('default',{}),('hip-waist',{'PBMHipSize':1.,'PBMWaistWidth':1.})]:
    points=base.copy()
    for morph in mesh['morph_targets']:
        amount=settings.get(morph['name'],0.)
        if amount:
            for index,*delta in morph['deltas']:points[index]+=np.asarray(delta)*amount
    cases[label]=points
reports=[]
for kind in ('walk','jog','sprint'):
    if args.clip and kind!=args.clip:continue
    frames=json.loads((work/f'follow-{kind}-base.json').read_text())['frames']
    selected=sorted(set(range(0,len(frames),12))|{len(frames)-1,6 if kind=='jog' else 20 if kind=='sprint' else 4})
    if args.frame is not None:selected=[args.frame]
    for fi in selected:
        snapshot=frames[fi]['pose']['Snapshot']
        local=dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
        pose=[]
        for bone in mesh['bones']:
            e=local[bone['name']]
            m=Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),Quaternion([e['Rotation'][k] for k in 'WXYZ']),Vector([e['Scale3D'][k] for k in 'XYZ']))
            if bone['parent']>=0:m=pose[bone['parent']]@m
            pose.append(m)
        mats=np.asarray([np.asarray(m@inv) for m,inv in zip(pose,inverse)])
        for label,points in cases.items():
            xyz=np.zeros_like(points)
            values=(np.einsum('nij,nj->ni',mats[bi,:3,:3],points[vi])+mats[bi,:3,3])*weights
            np.add.at(xyz,vi,values)
            tree=BVHTree.FromPolygons(xyz.tolist(),body_faces,all_triangles=True)
            hits=[];minimum=float('inf')
            for v in vertices:
                point=Vector(xyz[v]);nearest,normal,index,distance=tree.find_nearest(point)
                signed=(point-nearest).dot(normal)
                minimum=min(minimum,signed)
                if signed<-.1:hits.append({'vertex':v,'signed_cm':signed,'body_triangle':index})
            hits.sort(key=lambda h:h['signed_cm'])
            for hit in hits[:8]:
                vertex=hit['vertex']
                triangle=body_faces[hit['body_triangle']]
                hit['rest_position_cm']=mesh['points'][vertex]
                hit['garment_weights']=influences[vertex]
                hit['body_vertices']=[{'vertex':v,'rest_position_cm':mesh['points'][v],
                    'weights':influences[v]} for v in triangle]
            if args.rays:
                for hit in hits[:8]:
                    point=Vector(xyz[hit['vertex']]);hit['position_cm']=list(point)
                    ray_rows=[]
                    for axis in range(3):
                        for sign in (-1,1):
                            direction=Vector((0,0,0));direction[axis]=sign
                            origin=point.copy();count=0;first=None
                            for _ in range(64):
                                location,normal,face,distance=tree.ray_cast(origin,direction,500)
                                if location is None:break
                                if first is None:first=distance
                                count+=1;origin=location+direction*.0001
                            ray_rows.append({'axis':axis,'sign':sign,'crossings':count,'first_distance_cm':first,'truncated':count==64})
                    hit['rays']=ray_rows
            reports.append({'clip':kind,'frame':fi,'morph_case':label,'tested_vertices':len(vertices),
                'minimum_signed_cm':minimum,'over_1mm':len(hits),'worst_samples':hits[:8]})
    print(kind,'cases',len(reports),flush=True)
output.write_text(json.dumps({'scope':'Nearest-triangle signed distances for lower main-dress vertices on sampled recorded baseline component poses. Body normals oriented using exported normals. Diagnostic only: open/concave surfaces and intentional openings need visual review. Morph endpoints reuse recorded pose, without resimulating dynamics.',
    'body_triangles':len(body_faces),'excluded_arm_triangles':body_count-len(body_faces),
    'rest_height_interval_cm':[args.min_z if np.isfinite(args.min_z) else None,args.max_z],
    'cases':reports},indent=2)+'\n')
print('WORST', [{k:r[k] for k in ('clip','frame','morph_case','minimum_signed_cm','over_1mm')}
                 for r in sorted(reports,key=lambda r:r['minimum_signed_cm'])[:2]])

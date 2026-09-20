"""Fit the existing directional clearance model to V44B2's neutral skin.

Retains exact directional support via a 2D convex hull of segment radials.
This does not claim full skin coverage, dynamic-corrective coverage or native
execution. Mixed web tissue remains outside these finger-cylinder bounds.
"""
import hashlib
import json
import math
import os
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=Path(os.environ['CSS_CLEARANCE_FIT_DIR']).resolve()
assert OUT.parent==WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT/'model.json').exists()
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0,str(MOD/'tools'))
from export_seduxtress_eve import TO_UE,read_bones,fitted_mesh,EXPORT_SHAPES

blend=MOD/'work/CSS_SeduXtress_ArmRestV44B2.blend'
source_hash=hashlib.sha256(blend.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(blend),use_scripts=False)
rig=bpy.data.objects['SKEL_CSS_Base']
bones,bind=read_bones(MOD/rig['CSS_bind_pose'])
idx={b['name']:i for i,b in enumerate(bones)}
body=bpy.data.objects['Eve Body']
for shape in body.data.shape_keys.key_blocks:
    if shape.name in EXPORT_SHAPES: shape.value=0
mesh,_,_=fitted_mesh(body)
points=np.array([list(TO_UE@body.matrix_world@v.co) for v in mesh.vertices])
weights=[{body.vertex_groups[g.group].name:g.weight for g in v.groups} for v in body.data.vertices]

def hull_indices(points):
    order=sorted(range(len(points)),key=lambda i:(points[i][0],points[i][1]))
    def cross(a,b,c):
        ab=points[b]-points[a];ac=points[c]-points[a]
        return ab[0]*ac[1]-ab[1]*ac[0]
    def half(indices):
        result=[]
        for i in indices:
            while len(result)>=2 and cross(result[-2],result[-1],i)<=0: result.pop()
            result.append(i)
        return result
    return half(order)[:-1]+half(order[::-1])[:-1]

models=[]
checks=[]
for finger in ('thumb','index','middle','ring','pinky'):
    names=[f'{finger}_{s:02d}_l' for s in (1,2,3)]
    ids=[idx[n] for n in names]
    selected=[i for i,w in enumerate(weights) if sum(w.get(n,0) for n in names)>.98]
    tips=[i for i,w in enumerate(weights) if w.get(names[2],0)>.5]
    knots=np.array([list(bind[i].translation) for i in ids])
    direction=knots[2]-knots[1];direction/=np.linalg.norm(direction)
    length=float(max((points[tips]-knots[2])@direction));assert .2<length<5
    knots=np.vstack((knots,knots[2]+direction*length))
    radials=[[],[],[]]
    radii=[0.,0.,0.]
    samples=[0,0,0]
    for vi in selected:
        p=points[vi]
        options=[]
        for segment in range(3):
            d=knots[segment+1]-knots[segment]
            t=float((p-knots[segment])@d/(d@d))
            closest=knots[segment]+max(0,min(1,t))*d
            options.append((float(np.linalg.norm(p-closest)),segment,t))
        distance,segment,t=min(options)
        if segment==0 and t<.15: continue
        axis=knots[segment+1]-knots[segment];axis/=np.linalg.norm(axis)
        radial=p-knots[segment]-axis*float((p-knots[segment])@axis)
        local=bind[ids[segment]].to_quaternion().inverted()@Vector(radial)
        radials[segment].append(list(local))
        radii[segment]=max(radii[segment],distance)
        samples[segment]+=1
    assert all(samples) and all(.1<r<3 for r in radii)
    hulls=[]
    for segment,values in enumerate(radials):
        values=np.array(values)
        axis=(bind[ids[segment]].to_quaternion().inverted()@Vector(knots[segment+1]-knots[segment])).normalized()
        u=axis.orthogonal().normalized();v=axis.cross(u).normalized()
        uv=values@np.array([u,v]).T
        hull=values[hull_indices(uv)]
        # Preserve exact sampled support in 3D too, catching projected-plane
        # assumptions and quaternion transport errors in the hull reduction.
        directions=[]
        for n in range(720):
            angle=n*math.tau/720
            directions.append(np.array(u)*math.cos(angle)+np.array(v)*math.sin(angle))
        directions += [np.array(axis),-np.array(axis)]
        directions=np.array(directions)
        error=float(np.max(np.abs(np.max(values@directions.T,axis=0)-np.max(hull@directions.T,axis=0))))
        assert error<1e-5,(finger,segment,error)
        hulls.append(hull.tolist())
        checks.append(dict(finger=finger,segment=segment,radials=len(values),hull_vertices=len(hull),max_support_error_cm=error))
    models.append(dict(finger=finger,indices=ids,bones=names,knots=knots.tolist(),radii=radii,samples=samples,
                       radials_local=radials,hull_radials_local=hulls,
                       tip_local=list(bind[ids[2]].inverted()@Vector(knots[3]))))
normal=(bind[idx['middle_01_l']].translation-bind[idx['hand_l']].translation).cross(
        bind[idx['index_01_l']].translation-bind[idx['pinky_01_l']].translation).normalized()
normal_local=bind[idx['hand_l']].to_quaternion().inverted()@normal
assert hashlib.sha256(blend.read_bytes()).hexdigest()==source_hash
model=dict(schema=1,scope=__doc__,blend_sha256=source_hash,bones=bones,models=models,
           palm_normal_local=list(normal_local),margin_cm=.01,gradient_step_degrees=.05,
           iteration_limit=12,iteration_step_degrees=2,correction_limit_degrees=12)
(OUT/'model.json').write_text(json.dumps(model,indent=2)+'\n')
(OUT/'fit-checks.json').write_text(json.dumps(dict(checks=checks,max_support_error_cm=max(c['max_support_error_cm'] for c in checks),
    full_radials=sum(c['radials'] for c in checks),hull_radials=sum(c['hull_vertices'] for c in checks)),indent=2)+'\n')
print(json.dumps(dict(segments=len(checks),full_radials=sum(c['radials'] for c in checks),hull_radials=sum(c['hull_vertices'] for c in checks))),flush=True)

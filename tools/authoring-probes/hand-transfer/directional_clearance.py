"""B2 geometry-based finger splay and thumb-tip clearance, offline reference.

Finite differences inspect only the selected contact pair. Convex hull support
replaces redundant radial points; no full skin or weapon contact is inferred.
"""
import copy
import math
import numpy as np
from mathutils import Matrix, Quaternion, Vector


def distance(a,b,c,d):
    u=b-a;v=d-c;w=a-c
    aa=float(u@u);bb=float(u@v);cc=float(v@v);dd=float(u@w);ee=float(v@w)
    assert aa>1e-10 and cc>1e-10
    denom=aa*cc-bb*bb
    candidates=[]
    if denom>1e-12:
        s=(bb*ee-cc*dd)/denom;t=(aa*ee-bb*dd)/denom
        if 0<=s<=1 and 0<=t<=1: candidates.append((float(np.linalg.norm(w+s*u-t*v)),a+s*u,c+t*v))
    for s in (0.,1.):
        t=max(0.,min(1.,float((w+s*u)@v/cc)))
        candidates.append((float(np.linalg.norm(w+s*u-t*v)),a+s*u,c+t*v))
    for t in (0.,1.):
        s=max(0.,min(1.,float(-(w-t*v)@u/aa)))
        candidates.append((float(np.linalg.norm(w+s*u-t*v)),a+s*u,c+t*v))
    return min(candidates,key=lambda x:x[0])


def vector_rotation(angles):
    vector=Vector(angles);length=vector.length
    return Quaternion(vector/length,length) if length>1e-12 else Quaternion((1,0,0,0))


def solve(doc, model, stage, *, full_radials=False, legacy_transport=False):
    assert stage in ('fingers','thumb')
    bones=model['bones'];index={b['name'].lower():i for i,b in enumerate(bones)}
    snapshot=doc['pose']['Snapshot']
    assert [n.lower() for n in snapshot['BoneNames'][:len(bones)]]==list(index)
    local=[];world=[]
    for bone,t in zip(bones,snapshot['LocalTransforms'][:len(bones)],strict=True):
        translation=[t['Translation'][a] for a in 'XYZ'] if model.get('translation_policy')=='captured' else bone['translation']
        m=Matrix.LocRotScale(Vector(translation),Quaternion([t['Rotation'][a] for a in 'WXYZ']),
                             Vector([t['Scale3D'][a] for a in 'XYZ']))
        local.append(m);world.append(world[bone['parent']]@m if bone['parent']>=0 else m)
    models=model['models'][1:] if stage=='fingers' else model['models']
    radial_key='radials_local' if full_radials else 'hull_radials_local'
    hulls=[[np.array(v) for v in m[radial_key]] for m in models]
    if legacy_transport:
        # Diagnostic parity mode retains the original matrix operation order.
        hulls=[[np.array(v) for v in m['radials']] for m in models]
        legacy_bind=[Matrix(m) for m in model['legacy_bind']]
    ids=[m['indices'] for m in models]
    bases=[local[chain[0]].to_quaternion() for chain in ids]
    normal=world[index['hand_l']].to_quaternion()@Vector(model['palm_normal_local'])
    if model.get('legacy_palm_composition'):
        normal=world[index['hand_l']].to_quaternion()@Quaternion(model['legacy_hand_inverse'])@Vector(model['legacy_palm_normal'])
    axes=[world[bones[chain[0]]['parent']].to_quaternion().inverted()@normal for chain in ids]
    pairs=[(f,f+1,s,t) for f in range(3) for s in range(3) for t in range(3)] if stage=='fingers' else [(0,g,2,t) for g in range(1,5) for t in range(3)]

    def geometry(angles):
        all_points=[];rotations=[]
        for fi,chain in enumerate(ids):
            w=world[bones[chain[0]]['parent']]
            positions=[];rotations_row=[]
            for segment,bi in enumerate(chain):
                m=local[bi]
                if segment==0 and (stage=='fingers' or fi==0):
                    offset=Quaternion(axes[fi],float(angles[fi])) if stage=='fingers' else vector_rotation(angles)
                    m=Matrix.LocRotScale(m.translation,offset@bases[fi],m.to_scale())
                w=w@m
                positions.append(list(w.translation))
                transport=w@legacy_bind[bi].inverted() if legacy_transport else w
                rotations_row.append(np.array(transport.to_3x3()))
            positions.append(list(w@Vector(models[fi]['tip_local'])))
            all_points.append(np.array(positions));rotations.append(rotations_row)
        return all_points,rotations

    def gaps(angles, chosen=None):
        points,rotations=geometry(angles)
        values=[]
        selected=range(len(pairs)) if chosen is None else [chosen]
        for i in selected:
            f,g,s,t=pairs[i]
            separation,a,b=distance(points[f][s],points[f][s+1],points[g][t],points[g][t+1])
            assert separation>1e-6,'Crossed centerlines need a separate constraint'
            normal=(b-a)/separation
            rf=max(0.,float(np.max(hulls[f][s]@(rotations[f][s].T@normal))))
            rg=max(0.,float(np.max(hulls[g][t]@(rotations[g][t].T@(-normal)))))
            values.append(separation-rf-rg)
        return np.array(values)

    angles=np.zeros(4 if stage=='fingers' else 3)
    initial=gaps(angles)
    history=[]
    margin=model['margin_cm'];epsilon=math.radians(model['gradient_step_degrees'])
    limit=math.radians(model['correction_limit_degrees']);step=math.radians(model['iteration_step_degrees'])
    for iteration in range(model['iteration_limit']):
        values=gaps(angles);worst=int(np.argmin(values));gap=float(values[worst])
        if gap>=margin-.0001: break
        gradient=np.zeros(len(angles))
        for fi in (pairs[worst][:2] if stage=='fingers' else range(3)):
            plus=angles.copy();minus=angles.copy();plus[fi]+=epsilon;minus[fi]-=epsilon
            gradient[fi]=(gaps(plus,worst)[0]-gaps(minus,worst)[0])/(2*epsilon)
        norm=float(gradient@gradient)
        if norm<1e-8: break
        correction=(margin-gap)*gradient/norm
        magnitude=float(np.max(np.abs(correction))) if stage=='fingers' else float(np.linalg.norm(correction))
        angles+=correction*min(1.,step/max(magnitude,1e-12))
        if stage=='fingers': angles=np.clip(angles,-limit,limit)
        else: angles*=min(1.,limit/max(float(np.linalg.norm(angles)),1e-12))
        history.append(dict(pair=pairs[worst],gap_cm=gap))
    final=gaps(angles)
    candidate=copy.deepcopy(doc)
    for fi,chain in enumerate(ids):
        if stage=='thumb' and fi!=0: continue
        magnitude=abs(angles[fi]) if stage=='fingers' else float(np.linalg.norm(angles))
        if magnitude<=1e-12: continue
        offset=Quaternion(axes[fi],float(angles[fi])) if stage=='fingers' else vector_rotation(angles)
        q=(offset@bases[fi]).normalized()
        candidate['pose']['Snapshot']['LocalTransforms'][chain[0]]['Rotation']=dict(zip('XYZW',(q.x,q.y,q.z,q.w)))
    return candidate,dict(stage=stage,angles_radians=list(angles),initial_gap_cm=float(min(initial)),
        final_gap_cm=float(min(final)),iterations=len(history),proxy_clear=bool(min(final)>=margin-.0001),history=history)

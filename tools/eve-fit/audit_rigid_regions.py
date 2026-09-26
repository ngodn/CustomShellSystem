"""Measure rigid per-region body following before building regional collision."""
import argparse
import json
import sys
from pathlib import Path
import numpy as np
from mathutils import Matrix, Quaternion, Vector

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
mesh = json.loads((w/'cbody.mesh.json').read_text())
body = json.loads((w/'body-collider.json').read_text())
points = np.asarray(body['positions'])
assert np.array_equal(points,np.asarray(mesh['points']))
names = {b['name']:i for i,b in enumerate(mesh['bones'])}
bind = []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    bind.append(bind[bone['parent']]@local if bone['parent']>=0 else local)
dominant = np.asarray([names[max(row,key=lambda x:x[1])[0]] for row in body['weights']])
regions = dominant.copy()
for name,index in names.items():
    # Twist weights share a limb carrier; secondary glute motion keeps its own carrier.
    if '_twist_' in name:
        carrier = name.split('_twist_')[0]+'_'+name[-1]
        assert carrier in names
        regions[dominant==index] = names[carrier]
arm = np.asarray([sum(weight for name,weight in row if any(s in name for s in ('arm','hand','thumb','index','middle','ring','pinky'))) for row in body['weights']])
roi = np.flatnonzero((points[:,2]>=90)&(points[:,2]<=125)&(arm<.25))
influences = [(i,names[name],weight) for i,row in enumerate(body['weights']) for name,weight in row]
vi,bi,weight = map(np.asarray,zip(*influences,strict=True))
vi,bi = vi.astype(int),bi.astype(int)
morphed = points.copy()
for morph in mesh['morph_targets']:
    if morph['name'] in ('PBMHipSize','PBMWaistWidth'):
        for vertex,*delta in morph['deltas']:
            morphed[vertex] += delta
def skin(rest,matrices):
    result = np.zeros_like(rest)
    np.add.at(result,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],rest[vi])+matrices[bi,:3,3])*weight[:,None])
    return result
def summarize(error,ids):
    worst = int(ids[error[ids].argmax()])
    return {'samples':len(ids),'max_cm':float(error[worst]),'p95_cm':float(np.percentile(error[ids],95)),
            'over_3mm':int((error[ids]>.3).sum()),'worst_index':worst,
            'worst_rest_cm':points[worst].tolist(),'worst_weights':body['weights'][worst]}
rows = []
for clip in ('sprint','walk','jog'):
    motion = json.loads((w/f'follow-{clip}-base.json').read_text())
    for frame_index,frame in enumerate(motion['frames']):
        snapshot = frame['pose']['Snapshot']
        entries = dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
        pose = []
        for bone in mesh['bones']:
            t = entries[bone['name']]
            local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
            pose.append(pose[bone['parent']]@local if bone['parent']>=0 else local)
        matrices = np.asarray([np.asarray(p@b.inverted()) for p,b in zip(pose,bind,strict=True)])
        rigid = np.einsum('nij,nj->ni',matrices[regions,:3,:3],points)+matrices[regions,:3,3]
        for label,target in [('default',points),('hip-waist',morphed)]:
            expected = skin(target,matrices)
            error = np.linalg.norm(rigid-expected,axis=1)
            regional = []
            for region in np.unique(regions[roi]):
                ids = roi[regions[roi]==region]
                regional.append({'bone':mesh['bones'][region]['name'],**summarize(error,ids)})
            rows.append({'clip':clip,'frame':frame_index,'case':label,'skirt_region':summarize(error,roi),'regions':regional})
    print(clip,'complete',flush=True)
a.output.write_text(json.dumps({'scope':'Rigid dominant-weight region correspondence against original skinned body. Twist drivers merged into limb carrier, secondary glutes kept separate. Rest geometry stays unmorphed. No collider surface, containment or cloth acceptance.',
                               'body_points':len(points),'region_bones':sorted(mesh['bones'][i]['name'] for i in np.unique(regions[roi])),
                               'cases':rows},indent=2)+'\n')
print('completed',len(rows),'cases',flush=True)

"""Replay UE 5.6 cloth render-position mapping using exported native particles/normals."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--mesh',type=Path)
p.add_argument('--slot')
p.add_argument('--mapping',type=Path,required=True)
p.add_argument('--motion',type=Path,required=True)
p.add_argument('--proxy',type=Path,required=True)
p.add_argument('--verification',type=Path,required=True)
p.add_argument('--frame',type=int,required=True)
p.add_argument('--output',type=Path,required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
verification = json.loads(a.verification.read_text())
assert verification['passed']
assert verification['source_sha256'] == hashlib.sha256(a.motion.read_bytes()).hexdigest()
assert verification['proxy_sha256'] == hashlib.sha256(a.proxy.read_bytes()).hexdigest()
w = Path(__file__).resolve().parents[2]/'work/eve26'
mapping = json.loads(a.mapping.read_text())
motion = json.loads(a.motion.read_text())
assert mapping['asset'] == motion['asset']
proxy = json.loads(a.proxy.read_text())['slots'][a.slot or 'MI_CH_P_EVE_Christmas_01_01.001']
mesh = json.loads((a.mesh or w/'holiday.mesh.json').read_text())
source = json.loads(Path(motion['source_motion']).read_text())
snapshot = source['frames'][a.frame]['pose']['Snapshot']
entries = dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
bind,pose = [],[]
for b in mesh['bones']:
    q = b['rotation'];m = Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
    bind.append(bind[b['parent']]@m if b['parent']>=0 else m)
    t = entries[b['name']]
    m = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
    pose.append(pose[b['parent']]@m if b['parent']>=0 else m)
transforms = np.asarray([np.asarray(m@b.inverted()) for m,b in zip(pose,bind)])
names = {b['name']:i for i,b in enumerate(mesh['bones'])}
frame = motion['frames'][a.frame]
sim_positions,sim_normals = np.asarray(frame['positions_cm']),np.asarray(frame['normals'])
assert sim_positions.shape == sim_normals.shape == np.asarray(proxy['positions']).shape
rest_positions = np.asarray(proxy['positions'])
triangles = np.asarray(proxy['indices']).reshape((-1,3))
face_normals = np.cross(rest_positions[triangles[:,1]]-rest_positions[triangles[:,0]],rest_positions[triangles[:,2]]-rest_positions[triangles[:,0]])
lengths = np.linalg.norm(face_normals,axis=1)
assert (lengths>1e-8).all()
face_normals /= lengths[:,None]
rest_normals = np.zeros_like(rest_positions)
for column in range(3):np.add.at(rest_normals,triangles[:,column],face_normals)
lengths = np.linalg.norm(rest_normals,axis=1)
assert (lengths>1e-8).all()
rest_normals /= lengths[:,None]
# ClothAssetBuilder recalculates unweighted face-average normals; shader normals
# have the opposite sign to ClothMeshDesc, rather than the authored smooth normals.
sections = []
material_slots = {
    'XM_Decal':'MI_CH_P_EVE_Christmas_01_Decal.001',
    'XM_Dress01':'MI_CH_P_EVE_Christmas_01_01.001',
    'XM_Hair':'MI_EVE_HR_Christmas_01_Fur.001',
    'XM_Dress02':'MI_EVE_HR_15_Emissive1.001',
    'XM_Dress03':'MI_CH_P_EVE_Christmas_01_03.001',
}
for section, summary in zip(mapping['render_geometry']['sections'],mapping['sections'],strict=True):
    # The private import uses distinct material packages with identical short names.
    material = a.slot or material_slots[Path(summary['material']).parent.name]
    rest = np.asarray(section['positions']);count = len(rest)
    assert count == summary['vertices']
    data = np.asarray(section['mapping'])
    assert len(data)%count==0 and data.shape[1]==9
    data = data.reshape((count,-1,9));n = data.shape[1]
    ids = data[:,:,:3].astype(int);flags = data[:,:,3]
    valid = flags<65535
    assert np.all((ids[valid]>=0)&(ids[valid]<len(sim_positions)))
    ids = np.clip(ids,0,len(sim_positions)-1)
    bary = np.stack((data[:,:,4],data[:,:,5],1.-data[:,:,4]-data[:,:,5]),axis=2)
    distances = data[:,:,7]
    weight = np.where(valid,data[:,:,8] if n>1 else 1.,0.)
    total = weight.sum(axis=1)
    blend = np.where(valid,1.-flags/65535.,0.).sum(axis=1)/n
    blend[total<=1e-4] = 0.
    def resolve(positions,normals,skinned):
        mapped = np.sum(bary[:,:,:,None]*(positions[ids]+normals[ids]*distances[:,:,None,None]),axis=2)
        average = np.sum(mapped*weight[:,:,None],axis=1)/np.maximum(total[:,None],1e-4)
        return skinned*(1.-blend[:,None])+average*blend[:,None]
    influences = np.asarray([(v,names[bone],weight) for v,row in enumerate(section['weights']) for bone,weight in row])
    vi,bi,weights = influences[:,0].astype(int),influences[:,1].astype(int),influences[:,2]
    skinned = np.zeros_like(rest)
    np.add.at(skinned,vi,(np.einsum('nij,nj->ni',transforms[bi,:3,:3],rest[vi])+transforms[bi,:3,3])*weights[:,None])
    actual = resolve(sim_positions,sim_normals,skinned)
    assert np.isfinite(actual).all()
    reconstructed = resolve(rest_positions,rest_normals,rest)
    error = np.linalg.norm(reconstructed-rest,axis=1)
    sections.append({'material':material,'positions_cm':actual.tolist(),'indices':section['indices'],
        'mapping_influences':n,'simulated_vertices':int((blend>0).sum()),
        'rest_mapping_max_cm':float(error.max()),'rest_mapping_p99_cm':float(np.percentile(error,99))})
    print({k:v for k,v in sections[-1].items() if k not in ('positions_cm','indices')},flush=True)
out = {'scope':'UE 5.6 GpuSkinVertexFactory.ush position mapping replay from actual saved mapping and component particles/normals. Component scale one. Offline only; not game/render-thread readback, morph integration or material validation.',
    'frame':a.frame,'source_motion':motion['source_motion'],'asset':motion['asset'],
    'mapping_sha256':hashlib.sha256(a.mapping.read_bytes()).hexdigest(),
    'simulation_sha256':hashlib.sha256(a.motion.read_bytes()).hexdigest(),'sections':sections}
if 'diagnostic_mapping' in mapping:
    out['scope'] = 'Hypothetical modified mapping replay on unchanged native particles. Not a saved native mapping or deployed asset.'
    out['diagnostic_mapping'] = mapping['diagnostic_mapping']
a.output.write_text(json.dumps(out)+'\n')

"""Check rest reconstruction of saved cloth mappings without running another simulation."""
import argparse
import json
import sys
from pathlib import Path

import numpy as np

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--mapping',type=Path,required=True)
p.add_argument('--proxy',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--slot',default='MI_CH_P_EVE_Christmas_01_01.001')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
mapping = json.loads(a.mapping.read_text())
proxy = json.loads(a.proxy.read_text())['slots'][a.slot]
positions = np.asarray(proxy['positions'])
faces = np.asarray(proxy['indices']).reshape((-1,3))
normals = np.zeros_like(positions)
fn = np.cross(positions[faces[:,1]]-positions[faces[:,0]],positions[faces[:,2]]-positions[faces[:,0]])
fn /= np.linalg.norm(fn,axis=1)[:,None]
for column in range(3):np.add.at(normals,faces[:,column],fn)
normals /= np.linalg.norm(normals,axis=1)[:,None]
reports = []
for section,summary in zip(mapping['render_geometry']['sections'],mapping['sections'],strict=True):
    rest = np.asarray(section['positions'])
    records = np.asarray(section['mapping']).reshape((len(rest),-1,9))
    n = records.shape[1]
    active = records[:,:,3]<65535
    ids = records[:,:,:3].astype(int)
    assert np.all((ids[active]>=0)&(ids[active]<len(positions)))
    ids = np.clip(ids,0,len(positions)-1)
    bary = records[:,:,4:7].copy();bary[:,:,2] = 1.-bary[:,:,0]-bary[:,:,1]
    weights = np.where(active,records[:,:,8] if n>1 else 1.,0.)
    total = weights.sum(axis=1)
    reconstruction = np.sum(bary[:,:,:,None]*(positions[ids]+normals[ids]*records[:,:,7,None,None]),axis=2)
    average = np.sum(reconstruction*weights[:,:,None],axis=1)/np.maximum(total[:,None],1e-4)
    blend = np.where(active,1.-records[:,:,3]/65535.,0.).sum(axis=1)/n
    blend[total<=1e-4] = 0.
    error = np.linalg.norm((average-rest)*blend[:,None],axis=1)
    exact = np.linalg.norm(reconstruction-rest[:,None,:],axis=2) <= .01
    bounded = (bary.min(axis=2)>=-.25)&(bary.max(axis=2)<=1.25)
    relevant = active&(weights>0)
    assert np.isfinite(error).all()
    worst = np.argsort(error)[-8:][::-1]
    row = {'material':summary['material'],'vertices':len(rest),'influences':n,
        'max_cm':float(error.max()),'p99_cm':float(np.percentile(error,99)),
        'over_1mm':int((error>.1).sum()),
        'dynamic_without_accurate_influence':int(((blend>0)&~np.any(exact&relevant,axis=1)).sum()),
        'dynamic_without_accurate_bounded_influence':int(((blend>0)&~np.any(exact&bounded&relevant,axis=1)).sum()),
        'worst':[{'vertex':int(i),'error_cm':float(error[i]),'rest':rest[i].tolist(),
            'mapping':records[i].tolist()} for i in worst]}
    reports.append(row)
    print({k:v for k,v in row.items() if k!='worst'},flush=True)
a.output.write_text(json.dumps({'scope':'Rest reconstruction from saved mapping and proxy geometry using the pinned shader position formula. No dynamic or game acceptance.',
    'asset':mapping['asset'],'mapping':str(a.mapping),'proxy':str(a.proxy),'sections':reports},indent=2)+'\n')

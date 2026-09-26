"""Verify carrier skinning against the original graph after actual component execution."""
import json
from pathlib import Path
import numpy as np
from mathutils import Matrix,Quaternion,Vector

work=Path(__file__).resolve().parents[2]/'work/eve26'
mesh=json.loads((work/'holiday-hip-clean.mesh.json').read_text())
mapping=json.loads((work/'skirt-follow.json').read_text())['drivers']
indices={b['name']:i for i,b in enumerate(mesh['bones'])}
bind=[]
for bone in mesh['bones']:
    q=bone['rotation']
    m=Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    if bone['parent']>=0:m=bind[bone['parent']]@m
    bind.append(m)
inverse=[m.inverted() for m in bind]
probes=np.asarray([(0,0,0,1),(100,0,0,1),(0,100,0,1),(0,0,200,1)]).T
def component(frame):
    snapshot=frame['pose']['Snapshot']
    entries=dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
    result=[]
    for bone in mesh['bones']:
        e=entries[bone['name']]
        m=Matrix.LocRotScale(Vector([e['Translation'][k] for k in 'XYZ']),Quaternion([e['Rotation'][k] for k in 'WXYZ']),Vector([e['Scale3D'][k] for k in 'XYZ']))
        if bone['parent']>=0:m=result[bone['parent']]@m
        result.append(m)
    return result
report={}
for kind in ('walk','jog','sprint'):
    baseline=json.loads((work/f'follow-{kind}-base.json').read_text())
    candidate=json.loads((work/f'follow-{kind}-candidate.json').read_text())
    worst=0.;where=None
    for fi,(a,b) in enumerate(zip(baseline['frames'],candidate['frames'],strict=True)):
        original=component(a);actual=component(b)
        for target,driver in mapping.items():
            i=indices[target];j=indices[driver]
            delta=np.asarray(actual[i]@inverse[i])-np.asarray(original[j]@inverse[j])
            error=float(np.linalg.norm((delta@probes)[:3],axis=0).max())
            assert np.isfinite(error)
            if error>worst:worst=error;where={'frame':fi,'bone':target}
    report[kind]={'frames':len(candidate['frames']),'max_affine_probe_error_cm':worst,'worst':where,'pass':worst<.001}
out=work/'follow-skinning-verified.json';assert not out.exists()
out.write_text(json.dumps({'clips':report,'scope':'Four independent points compare each carrier skinning transform against its original driver after actual compressed animation and secondary graph execution. No cloth dynamics/contact acceptance.'},indent=2)+'\n')
print(json.dumps(report,indent=2))
assert all(r['pass'] for r in report.values()),'Carrier skinning differs from fitted baseline'

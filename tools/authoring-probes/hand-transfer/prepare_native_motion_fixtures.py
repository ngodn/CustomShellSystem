"""Interpolate original running-attack inputs before the complete native chain."""
import copy
import hashlib
import json
import os
import sys
from pathlib import Path
from mathutils import Quaternion

sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import WORK,load

out=Path(os.environ['CSS_NATIVE_HAND_MOTION_FIXTURES']).resolve()
assert out.parent==WORK.resolve()
out.mkdir(exist_ok=True);assert not (out/'fixtures.json').exists()
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
base_path=WORK/'hand-combined-fixtures-v1/fixtures.json'
base={c['label']:c for c in load(base_path)['cases']}
regression_path=WORK/'arm-rest-thumb-web-transitions-v2/report.json'
affected={(r['frame'],r['alpha']) for r in load(regression_path)['samples']}
assert len(affected)==15
times_path=WORK/'running-attack-dense-hand-v2/dense/animation-poses.json'
times={f['frame']:f['timeSeconds'] for f in load(times_path)['frames']}
assert len(times)==112
sources={str(p):digest(p) for p in (base_path,regression_path,times_path)}
cases=[]
for alpha in (0.,.5,1.):
    poses=[]
    for frame in range(112):
        source=base[f'synthetic-running_attack-{frame}-alpha-{alpha}.json']
        p=Path(source['source']);assert digest(p)==source['source_sha256']
        sources[str(p)]=digest(p);poses.append(load(p))
    for frame in range(112):
        fractions=[0.]
        if frame<111:fractions += [i/8 for i in range(1,8)] if (frame,alpha) in affected else [.5]
        for fraction in fractions:
            doc=copy.deepcopy(poses[frame]);time=times[frame]
            if fraction:
                time=times[frame]*(1-fraction)+times[frame+1]*fraction
                for dst,a,b in zip(doc['pose']['Snapshot']['LocalTransforms'],poses[frame]['pose']['Snapshot']['LocalTransforms'],poses[frame+1]['pose']['Snapshot']['LocalTransforms'],strict=True):
                    qa,qb=[Quaternion([t['Rotation'][k] for k in 'WXYZ']).normalized() for t in (a,b)]
                    q=qa.slerp(qb,fraction).normalized()
                    dst['Rotation']=dict(zip('XYZW',(q.x,q.y,q.z,q.w)))
                    for field in ('Translation','Scale3D'):
                        dst[field]={k:a[field][k]*(1-fraction)+b[field][k]*fraction for k in 'XYZ'}
            label=f'running-{frame}-{fraction}-alpha-{alpha}'
            path=out/(label+'.json');path.write_text(json.dumps(doc)+'\n')
            cases.append(dict(label=label,source=str(path),source_sha256=digest(path),
                              frame=frame,fraction=fraction,alpha=alpha,time_seconds=time,
                              v43_compatible=False,singular=False))
assert len(cases)==759 and sum(c['fraction']>0 for c in cases)==423
(out/'fixtures.json').write_text(json.dumps(dict(scope=__doc__,sampling='original-input-running-transitions-v1',
    sources=sources,cases=cases),indent=2)+'\n')
print('Prepared 759 native motion inputs, including 423 interior samples')

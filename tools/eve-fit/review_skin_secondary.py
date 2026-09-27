"""Review Skin F16 against recorded shared secondary poses. Python 3.14."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
work=root/'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--outfit',choices=('skin','bikini'),default='skin')
a=parser.parse_args()
bikini=a.outfit=='bikini'
out=work/(a.outfit+'-secondary');out.mkdir(exist_ok=False)
mesh=work/('bikini-import1/bikini.mesh.json' if bikini else 'skin-f16-export/skin.mesh.json')
source=json.loads(mesh.read_text())
blender=root.parent/'CSS-eins0fx-collections/reference-tools/blender/blender'
regions={'brust001','brust002','butt001','butt002','thigh_twist_02_l','thigh_twist_02_r','belly'}
summary=[]
for gait in ('walk','jog','sprint'):
    motion=work/(f'feet-{gait}-candidate.json' if bikini else f'feet-{gait}-base.json')
    data=json.loads(motion.read_text())
    scores=[]
    for i,frame in enumerate(data['frames']):
        pose=frame['pose']['Snapshot'];base=frame['upstream']
        assert pose['BoneNames']==base['BoneNames']
        assert {b['name'] for b in source['bones']}<=set(pose['BoneNames'])
        score=0
        for name,a,b in zip(pose['BoneNames'],pose['LocalTransforms'],base['LocalTransforms'],strict=True):
            if name not in regions:continue
            qa=[a['Rotation'][k] for k in 'XYZW'];qb=[b['Rotation'][k] for k in 'XYZW']
            dot=abs(sum(x*y for x,y in zip(qa,qb)))/math.sqrt(sum(x*x for x in qa)*sum(x*x for x in qb))
            score=max(score,math.degrees(2*math.acos(min(1,dot))))
        scores.append(score)
    index=max(range(len(scores)),key=scores.__getitem__)
    assert scores[index]>.001,'Recording has no body secondary rotation'
    args=[str(blender),'-b','--factory-startup','--python-exit-code','1','--python',str(root/'tools/eve-fit/review_outfit_export.py'),'--',
        '--mesh',str(mesh),'--audit',str(mesh.with_suffix('.audit.json')),'--output',str(out/gait),
        '--pose-motion',str(motion),'--pose-frame',str(index),'--post-process-pose']
    if not bikini:args+=['--hide-material','SkinCovered_1','--hide-material','SkinCovered_6']
    for name in ('FBMBodyTone','PBMBreastsSize','PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth'):
        args+=['--morph',name+'=1']
    with (out/f'{gait}.log').open('w') as log:subprocess.run(args,check=True,stdout=log,stderr=subprocess.STDOUT)
    summary.append(dict(gait=gait,frame=index,max_body_rotation_degrees=scores[index],recording_sha256=hashlib.sha256(motion.read_bytes()).hexdigest()))
(out/'receipt.json').write_text(json.dumps(dict(cases=summary,source_sha256=hashlib.sha256(mesh.read_bytes()).hexdigest(),
    scope='Maximum body secondary rotation frames replayed on the selected fitted outfit with all six morphs at maximum. Recorded shared graph poses, not a fresh Skin component simulation or game acceptance.'),indent=2)+'\n')
print(summary)

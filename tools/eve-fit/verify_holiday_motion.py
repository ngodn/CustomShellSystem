"""Reject catastrophic skirt motion even when transforms are finite and other bones are stable."""
import json
import math
import argparse
from pathlib import Path

work=Path(__file__).resolve().parents[2]/'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--candidate', choices=('holiday','holiday2'), default='holiday')
args=parser.parse_args()
reports={}
for kind in ('walk','jog','sprint'):
    path=work/f'{args.candidate}-{kind}-motion.json'
    if not path.exists():
        reports[kind]={'bounded':False,'missing':True}
        continue
    data=json.loads(path.read_text())
    worst_angle=(0,0,'')
    worst_translation=0.
    for index,frame in enumerate(data['frames']):
        pose=frame['pose']['Snapshot']
        upstream=frame['upstream']
        assert pose['BoneNames']==upstream['BoneNames']
        for name,actual,expected in zip(pose['BoneNames'],pose['LocalTransforms'],upstream['LocalTransforms'],strict=True):
            if not name.startswith('CSS_Cloth_Skirt_'):continue
            qa=[actual['Rotation'][axis] for axis in 'XYZW']
            qb=[expected['Rotation'][axis] for axis in 'XYZW']
            dot=sum(a*b for a,b in zip(qa,qb))/math.sqrt(sum(v*v for v in qa)*sum(v*v for v in qb))
            angle=math.degrees(2*math.acos(min(1,abs(dot))))
            if angle>worst_angle[0]:worst_angle=(angle,index,name)
            distance=math.dist([actual['Translation'][axis] for axis in 'XYZ'],[expected['Translation'][axis] for axis in 'XYZ'])
            assert math.isfinite(distance)
            worst_translation=max(worst_translation,distance)
    reports[kind]={'frames':len(data['frames']),'max_angle_deg':worst_angle[0],
        'frame':worst_angle[1],'bone':worst_angle[2], 'max_local_translation_cm':worst_translation,
        'bounded':worst_translation<10 and worst_angle[0]<60}
report={'clips':reports,'scope':'Coarse instability rejection only. Passing does not establish cloth fitting or gameplay acceptance.'}
(work/f'{args.candidate}-motion-verdict.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
assert all(c['bounded'] for c in reports.values()), 'Unstable Holiday solver candidate. Do not deploy.'

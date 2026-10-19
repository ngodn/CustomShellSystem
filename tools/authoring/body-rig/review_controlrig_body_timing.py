"""Check timed body measurements against geometry, clock and motion requirements.

Copied unchanged in its checks and tolerances from the SeduXtress authoring
library (review_controlrig_body_timing.py, sha256 00639f85...84d6). Only the
paths changed: it reads and writes in --stage, and --label selects a run.
"""
import argparse
import bisect
import hashlib
import json
import math
from pathlib import Path



def multiply(a,b):
    x,y,z,w=a; X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]


def unit(q):
    length=math.sqrt(sum(v*v for v in q))
    if not length>0 or not math.isfinite(length): raise ValueError('Invalid quaternion')
    return [v/length for v in q]


def angle(a,b):
    return math.degrees(2*math.acos(min(1,abs(sum(x*y for x,y in zip(unit(a),unit(b)))))))


def slerp(a,b,t):
    a,b=unit(a),unit(b)
    dot=sum(x*y for x,y in zip(a,b))
    if dot<0: b=[-v for v in b]; dot=-dot
    if dot>.9995: return unit([x+t*(y-x) for x,y in zip(a,b)])
    theta=math.acos(min(1,dot)); denominator=math.sin(theta)
    return [(math.sin((1-t)*theta)*x+math.sin(t*theta)*y)/denominator for x,y in zip(a,b)]


def rotations(sample):
    return [multiply(sample['owner']['rotation'],b['rotation']) for b in sample['body_pose']]


def comparison(case, reference):
    times=[s['time'] for s in reference['samples']]
    values=[rotations(s) for s in reference['samples']]
    largest=aligned=0.
    for sample in case['samples']:
        t=sample['time']; index=min(len(times)-1,bisect.bisect_left(times,t))
        if abs(times[index]-t)<1e-7:
            target=values[index]
            error=max(angle(a,b) for a,b in zip(rotations(sample),target))
            aligned=max(aligned,error)
        else:
            previous=max(0,index-1)
            alpha=(t-times[previous])/(times[index]-times[previous])
            target=[slerp(a,b,alpha) for a,b in zip(values[previous],values[index])]
            error=max(angle(a,b) for a,b in zip(rotations(sample),target))
        largest=max(largest,error)
    return dict(all_sample_degrees=largest,aligned_sample_degrees=aligned)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage',type=Path)
    parser.add_argument('--version',type=int,choices=(2,3,4,5,6,7,8,9,10,11),default=4)
    parser.add_argument('--label',default='')
    args=parser.parse_args()
    suffix='-'+args.label if args.label else ''
    source=args.stage/f'controlrig-body-angular-v{args.version}-timing{suffix}.json'
    output=args.stage/f'controlrig-body-angular-v{args.version}-timing{suffix}-review.json'
    if output.exists(): raise FileExistsError(output)
    data=json.loads(source.read_text()); cases={c['name']:c for c in data['cases']}
    if len(cases)!=37 or len(data['cases'])!=37: raise ValueError('Expected all 37 distinct fixtures')
    checks=[]
    def check(name,value,limit):
        checks.append(dict(name=name,value=value,maximum=limit,passed=math.isfinite(value) and value<=limit))
    for name,case in cases.items():
        last=case['samples'][-1]
        for field,limit in [('pivot_cm',.00001),('scale',.000001),('quaternion_norm_error',.000001),
                            ('limit_excess_degrees',.001),('substeps',8),('preview_steps',1)]:
            check(name+'/'+field,case['maximum'][field],limit)
        expected=0 if name in ('disabled','nonuniform_passthrough') else 418 if name=='reenable' else 476 if name=='scale_change' else 478 if name in ('teleport_automatic','epoch_reset','hitch','zero_delta') else 480
        resets=0 if not expected else 3 if name=='scale_change' else 2 if name in ('teleport_automatic','epoch_reset','hitch','zero_delta','reenable') else 1
        check(name+'/steps',abs(last['steps']-expected),0)
        check(name+'/resets',abs(last['resets']-resets),0)
        check(name+'/settling_degrees',max(last['angles_degrees']),.1)
        if expected:
            check(name+'/frozen_output',int(case['maximum']['angle_degrees']<.01),0)
        else:
            check(name+'/passthrough_degrees',case['maximum']['angle_degrees'],.001)
        if case.get('fps',60)>120 and expected:
            check(name+'/preview_state_checks_missing',int(case['preview_unchanged_frames']==0),0)
        prior_resets=0
        for sample in case['samples']:
            check(name+f"/frame_{sample['frame']}/remainder_underflow",max(0,-sample['remainder']),1e-6)
            check(name+f"/frame_{sample['frame']}/remainder_overflow",max(0,sample['remainder']-1/120),1e-6)
            if sample['resets']>prior_resets or not sample['enabled']:
                check(name+f"/frame_{sample['frame']}/reseed_degrees",max(sample['angles_degrees']),.001)
            prior_resets=sample['resets']
    comparisons={}
    for motion in ('translation','local','parent','smooth'):
        for fps in ((30,60,144,240) if motion=='smooth' else (15,30,60,144,240)):
            name=f'{motion}_{fps}'
            comparisons[name]=comparison(cases[name],cases[f'{motion}_120'])
            # Chosen before inspecting the V3 results. Piecewise-linear inputs
            # isolate clock behavior; smooth inputs also include pose sampling.
            check(name+'/all_sample_timing_degrees',comparisons[name]['all_sample_degrees'],.3 if motion=='smooth' else .25)
            if motion!='smooth':
                check(name+'/aligned_timing_degrees',comparisons[name]['aligned_sample_degrees'],.05)
    for name,reference,limit in [('jitter','translation_120',.25),
        ('component_translation','translation_60',.02),('component_parent','parent_60',.02),
        ('far_origin','translation_60',.02),('uniform_scale','smooth_60',.02),('duplicate','translation_144',.001)]:
        comparisons[name]=comparison(cases[name],cases[reference])
        check(name+'/equivalence_degrees',comparisons[name]['all_sample_degrees'],limit)
    result=dict(passed=all(c['passed'] for c in checks),checks=checks,comparisons=comparisons,
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),production_accepted=False,
        scope='Direct native timed body rig, 37 fixtures. Reflected committed-state identity during pure preview is checked by the harness. No contacts, AnimBP, cook or live acceptance.')
    output.write_text(json.dumps(result,indent=2)+'\n')
    failures=[c for c in checks if not c['passed']]
    print(f'{len(checks)-len(failures)}/{len(checks)} checks passed')
    print(json.dumps(failures[:45],indent=2))
    if failures: raise SystemExit(1)


if __name__=='__main__': main()

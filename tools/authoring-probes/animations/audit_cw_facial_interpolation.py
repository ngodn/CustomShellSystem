"""Compare baked quarter-weight CW controls with independent source evaluations."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baked-samples',type=Path,required=True)
    parser.add_argument('--reference-samples',type=Path,required=True)
    parser.add_argument('--max-error-cm',type=float,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists() or not np.isfinite(args.max_error_cm) or args.max_error_cm<=0:
        raise ValueError('Use a new report and a finite positive error limit')
    protected={}
    for folder in (args.baked_samples,args.reference_samples):
        for path in [folder/'report.json',*sorted(folder.glob('*.npz'))]:
            protected[str(path.resolve())]=digest(path)
    baked=json.loads((args.baked_samples/'report.json').read_text())
    reference=json.loads((args.reference_samples/'report.json').read_text())
    if baked['sha256']!=reference['sha256'] or not all(r['sources_unchanged'] for r in (baked,reference)):
        raise ValueError('Source identity or preservation differs')

    def load(folder,label):
        with np.load(folder/(label+'.npz')) as data:
            result={name:data[name].copy() for name in data.files}
        if any(a.ndim!=2 or a.shape[1]!=3 or not np.isfinite(a).all() for a in result.values()):
            raise ValueError('Invalid source positions')
        return result

    neutral=load(args.baked_samples,'neutral')
    other=load(args.reference_samples,'neutral')
    if set(neutral)!=set(other) or any(not np.array_equal(neutral[p],other[p]) for p in neutral):
        raise ValueError('Independent neutral sample differs')
    controls={r['control'] for r in baked['summary']}
    if controls!={r['control'] for r in reference['summary']}:
        raise ValueError('Control coverage differs')
    keys={}
    for name in controls:
        levels=sorted(r['value'] for r in baked['summary'] if r['control']==name)
        if levels!=[.25,.5,.75,1.0]:
            raise ValueError('Expected four quarter-weight samples')
        keys[name]=[neutral,*[load(args.baked_samples,f'{name}-{value:g}') for value in levels]]

    def delta(name,value):
        if name not in keys or not np.isfinite(value) or not 0<=value<=1:
            raise ValueError('Invalid interpolation control')
        lower=min(int(value*4),3)
        alpha=value*4-lower
        return {p:keys[name][lower][p]*(1-alpha)+keys[name][lower+1][p]*alpha-neutral[p]
                for p in neutral}

    def compare(expected,actual):
        parts={}
        for name in neutral:
            if expected[name].shape!=actual[name].shape:
                raise ValueError('Sample topology differs')
            error=np.linalg.norm(expected[name]-actual[name],axis=1)
            parts[name]=dict(max_error_cm=float(error.max()),rms_error_cm=float(np.sqrt(np.mean(error**2))),
                             worst_vertex=int(error.argmax()))
        return dict(max_error_cm=max(p['max_error_cm'] for p in parts.values()),parts=parts)

    records=[]
    for row in reference['summary']:
        label=f'{row["control"]}-{row["value"]:g}'
        d=delta(row['control'],row['value'])
        actual=load(args.reference_samples,label)
        records.append(dict(label=label,kind='single',**compare({p:neutral[p]+d[p] for p in neutral},actual)))
    for row in reference['combinations']:
        expected={p:a.copy() for p,a in neutral.items()}
        additive={p:a.copy() for p,a in neutral.items()}
        for name,value in row['controls'].items():
            d=delta(name,value)
            exact=load(args.reference_samples,f'{name}-{value:g}')
            for p in neutral:
                expected[p]+=d[p]
                additive[p]+=exact[p]-neutral[p]
        actual=load(args.reference_samples,row['label'])
        records.append(dict(label=row['label'],kind='combined',**compare(expected,actual),
                            interaction=compare(additive,actual)))
    if not records or not reference['combinations']:
        raise ValueError('Missing independent controls or combination coverage')
    if any(digest(Path(path))!=sha for path,sha in protected.items()):
        raise ValueError('A sample changed during comparison')
    maximum=max(r['max_error_cm'] for r in records)
    report=dict(complete=True,passed=maximum<=args.max_error_cm,max_error_cm=maximum,
                limit_cm=args.max_error_cm,records=records,protected=protected,
                scope='Source geometry at sampled intermediate control strengths and combinations. Cooked morph filtering and runtime playback are not included.')
    args.output.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print('CW_FACE_INTERPOLATION',report['passed'],maximum,flush=True)
    if not report['passed']:
        raise SystemExit(1)


if __name__=='__main__':
    main()

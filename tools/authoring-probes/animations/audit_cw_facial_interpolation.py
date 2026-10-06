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
    parser.add_argument('--corrective-samples',type=Path,help='Paired source control grids for a corrective trial')
    parser.add_argument('--blink-bias',type=Path,help='Audited expression-driven blink fractions for normalized grids')
    parser.add_argument('--max-error-cm',type=float,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists() or not np.isfinite(args.max_error_cm) or args.max_error_cm<=0:
        raise ValueError('Use a new report and a finite positive error limit')
    protected={}
    blink_bias={}
    if args.blink_bias:
        if not args.corrective_samples:
            raise ValueError('Blink normalization requires corrective samples')
        manifest=json.loads(args.blink_bias.read_text())
        protected[str(args.blink_bias.resolve())]=digest(args.blink_bias)
        for path,sha in manifest['protected'].items():
            if digest(Path(path))!=sha:
                raise ValueError('Blink driver evidence changed')
            protected[path]=sha
        blink_bias=manifest['controls']
        if not blink_bias or any(type(v) not in (float,int) or not 0<v<1 for v in blink_bias.values()):
            raise ValueError('Invalid expression blink fraction')
    folders=[args.baked_samples,args.reference_samples]
    if args.corrective_samples:
        folders.append(args.corrective_samples)
    for folder in folders:
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

    def normalized_pair(pair,values):
        point=list(values)
        for i,name in enumerate(pair):
            if name in blink_bias:
                other=1-i
                if pair[other] not in ('facs_jnt_EyeBlinkLeft','facs_jnt_EyeBlinkRight'):
                    raise ValueError('Blink bias paired with a different control')
                point[other]=min(values[other]/(1-blink_bias[name]*values[i]),1)
        return point

    correctives={}
    if args.corrective_samples:
        combined=json.loads((args.corrective_samples/'report.json').read_text())
        if combined['sha256']!=baked['sha256'] or not combined['sources_unchanged']:
            raise ValueError('Corrective source identity differs')
        base=load(args.corrective_samples,'neutral')
        if set(base)!=set(neutral) or any(not np.array_equal(base[p],neutral[p]) for p in neutral):
            raise ValueError('Corrective neutral differs')
        for row in combined['combinations']:
            pair=tuple(sorted(row['controls']))
            if len(pair)!=2 or any(n not in keys or not 0<row['controls'][n]<=1 for n in pair):
                raise ValueError('Expected positive paired controls')
            actual_values=[row['controls'][n] for n in pair]
            normalized=normalized_pair(pair,actual_values)
            point=tuple(round(value*4)/4 for value in normalized)
            if any(value not in (.25,.5,.75,1) or abs(value-original)>1e-8 for value,original in zip(point,normalized)):
                raise ValueError('Expected quarter-strength corrective grid coordinates')
            grid=correctives.setdefault(pair,{})
            if point in grid:
                raise ValueError('Duplicate corrective grid point')
            actual=load(args.corrective_samples,row['label'])
            terms=[delta(name,value) for name,value in zip(pair,actual_values)]
            grid[point]={p:actual[p]-neutral[p]-terms[0][p]-terms[1][p] for p in neutral}
        for grid in correctives.values():
            axes=[sorted({point[i] for point in grid}) for i in (0,1)]
            if any(axis[-1]!=1 for axis in axes) or set(grid)!={(a,b) for a in axes[0] for b in axes[1]}:
                raise ValueError('Incomplete corrective grid')

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
        uncorrected=compare(expected,actual)
        active_bias=[n for n in blink_bias if row['controls'].get(n,0)>0]
        if len(active_bias)>1:
            raise ValueError('Simultaneous biased expressions need separate source validation')
        if active_bias:
            name=active_bias[0]
            remaining=1-blink_bias[name]*row['controls'][name]
            for eye in ('facs_jnt_EyeBlinkLeft','facs_jnt_EyeBlinkRight'):
                value=row['controls'].get(eye,0)
                old,new=delta(eye,value),delta(eye,min(value,remaining))
                for p in neutral:
                    expected[p]+=new[p]-old[p]
        for pair,grid in correctives.items():
            strength=normalized_pair(pair,[row['controls'].get(name,0) for name in pair])
            if not all(strength):
                continue
            brackets=[]
            for i,value in enumerate(strength):
                axis=[0,*sorted({point[i] for point in grid})]
                upper=min(int(np.searchsorted(axis,value,side='right')),len(axis)-1)
                low,high=axis[upper-1:upper+1]
                alpha=(value-low)/(high-low)
                brackets.append([(low,1-alpha),(high,alpha)])
            for a,weight_a in brackets[0]:
                for b,weight_b in brackets[1]:
                    if not a or not b:
                        continue
                    for p in neutral:
                        expected[p]+=weight_a*weight_b*grid[a,b][p]
        records.append(dict(label=row['label'],kind='combined',**compare(expected,actual),
                            interaction=compare(additive,actual),uncorrected=uncorrected))
    if not records or not reference['combinations']:
        raise ValueError('Missing independent controls or combination coverage')
    if any(digest(Path(path))!=sha for path,sha in protected.items()):
        raise ValueError('A sample changed during comparison')
    maximum=max(r['max_error_cm'] for r in records)
    report=dict(complete=True,passed=maximum<=args.max_error_cm,max_error_cm=maximum,
                limit_cm=args.max_error_cm,records=records,protected=protected,
                corrective_pairs=[dict(controls=pair,points=list(grid)) for pair,grid in correctives.items()],
                blink_bias=blink_bias,
                scope='Source geometry at sampled intermediate control strengths and combinations. Cooked morph filtering and runtime playback are not included.')
    args.output.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print('CW_FACE_INTERPOLATION',report['passed'],maximum,flush=True)
    if not report['passed']:
        raise SystemExit(1)


if __name__=='__main__':
    main()

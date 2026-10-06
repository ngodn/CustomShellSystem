"""Compare quarter-sample CW performance controls with independent source poses."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from audit_cw_facial_interpolation import interpolation_weights


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    protected = {}

    def protect(path):
        path = path.resolve(strict=True)
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream,'sha256').hexdigest()
        if protected.setdefault(str(path),checksum)!=checksum:
            raise ValueError('Input changed: '+str(path))
        return path

    def read(path):
        return json.loads(protect(path).read_text())

    work = args.work.resolve(strict=True)
    fit = read(work/'cw-retained-basis-v1/report.json')
    transfer = read(work/'cw-performance-fitted-v1/report.json')
    if not fit.get('complete') or not transfer.get('complete'):
        raise ValueError('Require the verified fitted control representation')
    for path,checksum in transfer['protected'].items():
        if protected[str(protect(Path(path)))]!=checksum:
            raise ValueError('Verified fitted input changed: '+path)
    new_path = work/'cw-performance-fitted-v1/facial-shapes.json'
    payload = read(new_path)
    if protected[str(new_path.resolve())]!=transfer['sha256']:
        raise ValueError('Fitted payload changed')
    old = read(work/'cw-expression-compact-v2/facial-shapes.json')
    with np.load(protect(work/'cw-retained-basis-v1/basis.npz')) as data:
        active = data['point_indices'].copy()
        coefficients = np.concatenate((data['existing_weights'],data['weights']/data['gains']),axis=1)
    parts = fit['parts']
    slot = {index:i for i,index in enumerate(payload['point_indices'])}
    names = fit['existing_names']+[shape['name'] for shape in payload['morph_targets']]
    shapes = np.zeros((len(names),len(active),3))
    for source in (old,payload):
        for shape in source['morph_targets']:
            if shape['name'] not in names:
                continue
            i = names.index(shape['name'])
            for point,x,y,z in shape['deltas']:
                index = source['point_indices'][point]
                if index in slot:
                    shapes[i,slot[index]] = [x,-y,z]
    shapes = shapes.astype(np.float32).astype(float)
    shapes[np.linalg.norm(shapes,axis=2)<=.015] = 0
    labels = {label:i for i,label in enumerate(fit['labels'])}
    samples = []
    for folder,source in [('cw-performance-holdout-v1','cw-performance-controls-v1'),
                          ('cw-tongue-holdout-v1','cw-tongue-fk-samples-v1')]:
        if read(work/folder/'exit.json')['exit_code']!=0:
            raise ValueError('Incomplete source sampling')
        audit = read(work/folder/'samples/report.json')
        if not audit.get('sources_unchanged'):
            raise ValueError('Source restoration did not pass')
        protect(Path(audit['source']))
        if protected[str(Path(audit['source']).resolve())]!=audit['sha256']:
            raise ValueError('Original character source changed')
        with np.load(protect(work/folder/'samples/neutral.npz')) as data:
            neutral = np.concatenate([data[name] for name in parts])
        for row in audit['summary']:
            control,value = row['control'],row['value']
            if value in (0,.25,.5,.75,1):
                raise ValueError('Holdout reuses a fitted quarter sample')
            ids = [labels[f'{source}/{control}-{v:g}'] for v in (.25,.5,.75,1)]
            blend = np.array([w for _,w in interpolation_weights((0,.25,.5,.75,1),value,'cubic')][1:])
            weights = blend@coefficients[ids]
            expected_path = work/folder/'samples'/f'{control}-{value:g}.npz'
            with np.load(protect(expected_path)) as data:
                expected = np.concatenate([data[name] for name in parts])-neutral
            actual = np.zeros_like(expected)
            actual[active] = (weights@shapes.reshape(len(names),-1)).reshape(-1,3)
            error = float(np.linalg.norm(actual-expected,axis=1).max())
            maximum_weight = float(np.abs(weights).max())
            samples.append(dict(control=control,value=value,error_cm=error,
                max_weight=maximum_weight,passed=error<=.015 and maximum_weight<=4))
    if len(samples)!=144:
        raise ValueError('Require four independent levels for all 36 sampled controls')
    for path in list(protected):
        protect(Path(path))
    passed = all(row['passed'] for row in samples)
    result = dict(passed=passed,protected=protected,samples=samples,
        max_error_cm=max(row['error_cm'] for row in samples),limit_cm=.015,
        controls=len({row['control'] for row in samples}),
        scope='Interpolated isolated source controls, with stored float32 deltas and morph filtering. Cross-character semantics, combined controls, cooked readback and live appearance remain unverified.')
    args.output.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    print('CW_PERFORMANCE_HOLDOUT',passed,result['max_error_cm'],
          'failed controls:',sorted({r['control'] for r in samples if not r['passed']}),flush=True)
    raise SystemExit(0 if passed else 1)


if __name__=='__main__':
    main()

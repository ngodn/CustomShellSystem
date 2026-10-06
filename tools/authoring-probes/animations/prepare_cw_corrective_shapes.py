"""Map a verified compact CW correction basis to its unchanged fitted mesh."""
import argparse
import json
from pathlib import Path

import numpy as np
from audit_cw_facial_interpolation import digest
from cw_facial_basis import filtered_shapes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--basis',type=Path,required=True)
    parser.add_argument('--baked',type=Path,required=True)
    parser.add_argument('--acceptance',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shape-gain',type=float,default=1)
    args=parser.parse_args()
    if args.output.exists():raise FileExistsError(args.output)
    paths=[args.basis/'basis.json',args.basis/'basis.npz',args.baked/'facial-shapes.json',
           args.baked/'report.json',args.acceptance]
    protected={str(p.resolve()):digest(p) for p in paths}
    metadata=json.loads(paths[0].read_text())
    shapes=json.loads(paths[2].read_text())
    bake=json.loads(paths[3].read_text())
    acceptance=json.loads(args.acceptance.read_text())
    if (not acceptance.get('passed') or acceptance.get('compression')!=metadata or
            shapes.get('schema')!=2 or not bake.get('sources_unchanged') or
            acceptance.get('shape_gain',1)!=args.shape_gain):
        raise ValueError('Require matching passed geometry and compression evidence')
    protected.update(metadata['protected']);protected.update(bake['protected'])
    if any(digest(Path(p))!=sha for p,sha in protected.items()):
        raise ValueError('Protected facial source changed')
    with np.load(paths[1]) as data:
        basis=data['basis'].copy();coefficients=data['coefficients'].copy()
    if (basis.shape!=(metadata['shapes'],sum(map(len,metadata['points'].values())),3) or
            coefficients.shape!=(metadata['samples'],metadata['shapes']) or
            not np.isfinite(basis).all() or not np.isfinite(coefficients).all() or
            np.abs(coefficients).max()>1 or len(shapes['morph_targets'])+len(basis)>128):
        raise ValueError('Invalid or excessive compact basis')
    indices={index:i for i,index in enumerate(shapes['point_indices'])}
    mapped=[]
    for part,points in metadata['points'].items():
        offset=bake['offsets'][part]
        if offset>=bake['offsets']['Commander White Top Mesh']:
            offset+=bake['topology_insertion_points']
        for point in points:
            if offset+point not in indices:
                raise ValueError('Correction extends beyond the verified fitted point map')
            mapped.append(indices[offset+point])
    if len(set(mapped))!=len(mapped):raise ValueError('Duplicate correction point mapping')
    added=[]
    for i,values in enumerate(basis):
        name=f'CSSF_CW_Correction_{i:03d}'
        deltas=[[mapped[j],float(v[0]),float(-v[1]),float(v[2])]
                for j,v in enumerate(values) if np.linalg.norm(v)>1e-9]
        if not deltas:raise ValueError('Empty correction shape')
        shapes['morph_targets'].append(dict(name=name,deltas=deltas))
        added.append(name)
    if len({v['name'] for v in shapes['morph_targets']})!=len(shapes['morph_targets']):
        raise ValueError('Duplicate shape names')
    gains={}
    for shape in shapes['morph_targets']:
        values=np.asarray([d[1:] for d in shape['deltas']])
        _,gain=filtered_shapes(values[None],0,args.shape_gain)
        gains[shape['name']]=float(gain[0])
        for delta in shape['deltas']:
            delta[1:]=[value*gain[0] for value in delta[1:]]
    args.output.mkdir(parents=True)
    target=args.output/'facial-shapes.json'
    target.write_text(json.dumps(shapes,separators=(',',':'),allow_nan=False)+'\n')
    # Static presets use exact source endpoints. Animation-curve mapping is
    # authored separately and must include the fitted correction weights.
    names={'Afraid':'Afraid','Angry':'Angry','Flirting':'Flirting','Frown':'Frown',
           'Shock':'Shock','SmileFullFace':'Smile','SmileOpenFullFace':'Open Smile',
           'Surprised':'Surprised'}
    expressions=[dict(id='cw.neutral',name='Neutral',weights={})]
    known={s['name'] for s in shapes['morph_targets']}
    for source,label in names.items():
        morph=f'CSSF_ctrl_{source}_Q4'
        if morph not in known:raise ValueError('Missing source expression endpoint')
        expressions.append(dict(id='cw.'+source.lower(),name=label,weights={morph:1/gains[morph]}))
    (args.output/'expressions.json').write_text(json.dumps(expressions,indent=2)+'\n')
    if any(digest(Path(p))!=sha for p,sha in protected.items()):
        raise ValueError('Protected facial source changed during preparation')
    report=dict(complete=True,protected=protected,shapes=len(shapes['morph_targets']),
                correction_shapes=added,expressions=len(names),neutral=True,
                shape_gains=gains,
                sha256=digest(target),installed=False,
                scope='Sparse fitted interchange and static preset definitions. Native import, cooking, facial performance mapping and live acceptance pending.')
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('CW_COMPACT_SHAPES',report['shapes'],report['expressions'],flush=True)


if __name__=='__main__':main()

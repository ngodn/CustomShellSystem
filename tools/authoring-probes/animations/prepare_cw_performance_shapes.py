"""Transfer the retained-control CW facial basis onto the verified fitted mesh."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    protected = {}

    def protect(path):
        path = path.resolve(strict=True)
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        if protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Source changed: '+str(path))
        return path

    def read(path):
        return json.loads(protect(path).read_text())

    work = args.work.resolve(strict=True)
    report = read(work/'cw-retained-basis-v1/report.json')
    bake = read(work/'cw-expression-bake-v1/report.json')
    if not report.get('complete') or not bake.get('sources_unchanged'):
        raise ValueError('Missing basis or fitted-source verification')
    for evidence in (report, bake):
        for path, checksum in evidence['protected'].items():
            if protected[str(protect(Path(path)))] != checksum:
                raise ValueError('Verified source no longer matches: '+path)
    with np.load(protect(work/'cw-retained-basis-v1/basis.npz')) as data:
        basis = data['basis'].copy()
        gains = data['gains'].copy()
        weights = data['weights'].copy()
        previous_weights = data['existing_weights'].copy()
        active = data['point_indices'].copy()
    names = report['existing_names']
    if (basis.shape != (59,5918,3) or gains.shape != (59,) or
            weights.shape != (144,59) or previous_weights.shape != (144,20) or
            active.shape != (5918,) or active.dtype.kind not in 'iu' or
            len(set(active.tolist())) != len(active) or len(names) != 20):
        raise ValueError('Unexpected retained-control basis dimensions')
    if (not all(np.isfinite(x).all() for x in (basis,gains,weights,previous_weights)) or
            np.any(gains < 1) or np.any(gains > 32)):
        raise ValueError('Invalid basis values or storage gains')
    parts = report['parts']
    counts = report['part_counts']
    with np.load(protect(work/'cw-performance-controls-v1/samples/neutral.npz')) as data:
        neutral = np.concatenate([data[name] for name in parts])
    if (neutral.shape != (sum(counts.values()),3) or not np.isfinite(neutral).all() or
            active.min() < 0 or active.max() >= len(neutral)):
        raise ValueError('Basis points do not match the original neutral geometry')
    source_to_assembly = []
    for name in parts:
        offset = bake['offsets'][name]
        if offset >= bake['offsets']['Commander White Top Mesh']:
            offset += bake['topology_insertion_points']
        source_to_assembly.extend(range(offset,offset+counts[name]))
    indices = np.asarray(source_to_assembly)[active]
    if len(set(indices.tolist())) != len(indices):
        raise ValueError('Ambiguous fitted point mapping')
    assembly_path = next(Path(p) for p in bake['protected'] if p.endswith('.mesh.json.gz'))
    with gzip.open(protect(assembly_path),'rt') as stream:
        assembly = json.load(stream)
    if len(assembly['points']) != bake['assembly_points']+bake['topology_insertion_points']:
        raise ValueError('Fitted topology changed')
    points = neutral[active]*[1,-1,1]
    correspondence = float(np.linalg.norm(np.asarray(assembly['points'])[indices]-points,axis=1).max())
    if correspondence > .0002:
        raise ValueError(f'Fitted correspondence differs by {correspondence} cm')
    del assembly

    old = read(work/'cw-expression-compact-v2/facial-shapes.json')
    old_shapes = {row['name']:row for row in old['morph_targets']}
    slot = {int(index):i for i,index in enumerate(indices)}
    existing = np.zeros((20,len(active),3))
    for i,name in enumerate(names):
        for point,x,y,z in old_shapes[name]['deltas']:
            index = old['point_indices'][point]
            if index in slot:
                existing[i,slot[index]] = [x,y,z]
    existing[np.linalg.norm(existing,axis=2)<=.015] = 0
    stored = (basis*gains[:,None,None]*[1,-1,1]).astype(np.float32)
    if np.linalg.norm(stored,axis=2).max() > 20:
        raise ValueError('A stored delta exceeds the unchanged importer bound')
    filtered = np.where((np.linalg.norm(stored,axis=2)>.015)[:,:,None],stored,0)
    playback = weights/gains
    if max(np.abs(playback).max(),np.abs(previous_weights).max()) > 4:
        raise ValueError('Stored weights exceed runtime bounds')
    residuals = []
    for i,label in enumerate(report['labels']):
        folder,sample = label.split('/',1)
        with np.load(protect(work/folder/'samples/neutral.npz')) as data:
            reference = {name:data[name].copy() for name in parts}
        with np.load(protect(work/folder/'samples'/(sample+'.npz'))) as data:
            expected = np.concatenate([data[name]-reference[name] for name in parts])*[1,-1,1]
        actual = previous_weights[i]@existing.reshape(20,-1)+playback[i]@filtered.reshape(59,-1)
        reconstructed = np.zeros_like(expected)
        reconstructed[active] = actual.reshape(-1,3)
        error = float(np.linalg.norm(expected-reconstructed,axis=1).max())
        if error > .015:
            raise ValueError(f'{label}: fitted control differs by {error} cm')
        residuals.append(dict(sample=label,error_cm=error))
    catalog = read(args.catalog)
    rig = next(r for r in catalog['catalog']['animation_library']['faces']
               if r['id']=='css.commanderwhite.face')
    used = {name for expression in rig['expressions'] for name,value in expression['weights'].items() if value}
    used.update(curve['morph'] for p in rig['performances'] for curve in p['curves'])
    if not used.issubset(names):
        raise ValueError('A currently used shape was omitted from the retained controls')
    shapes = []
    for i,values in enumerate(stored):
        selected = np.flatnonzero(np.any(values!=0,axis=1))
        if not len(selected):
            raise ValueError('Empty added shape')
        shapes.append(dict(name=f'CSSF_CW_Performance_{i:03}',deltas=[
            [int(j),*values[j].tolist()] for j in selected]))
    payload = dict(schema=2,point_indices=indices.tolist(),body_points=points.tolist(),morph_targets=shapes)
    encoded = json.dumps(payload,separators=(',',':'),allow_nan=False)+'\n'
    if len(encoded.encode()) > 64*1024*1024:
        raise ValueError('Payload exceeds the unchanged importer limit')
    for path in list(protected):
        protect(Path(path))
    args.output.mkdir(parents=True)
    target = args.output/'facial-shapes.json'
    target.write_text(encoded)
    with target.open('rb') as stream:
        checksum = hashlib.file_digest(stream,'sha256').hexdigest()
    result = dict(complete=True,protected=protected,sources_unchanged=True,sha256=checksum,
        added_shapes=59,retained_catalog_shapes=names,catalog_shapes=79,
        existing_used_shapes=sorted(used),affected_points=len(active),
        correspondence_error_cm=correspondence,source_samples=residuals,
        max_source_error_cm=max(r['error_cm'] for r in residuals),
        previous_morphs_preserved=True,installed=False,
        scope='Fitted point transfer and isolated source-control reconstruction with the existing morph filter. Cross-character mappings, combined motion, native import, cook and gameplay remain unverified.')
    (args.output/'report.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    print('CW_PERFORMANCE_SHAPES',len(shapes),correspondence,result['max_source_error_cm'],flush=True)


if __name__=='__main__':
    main()

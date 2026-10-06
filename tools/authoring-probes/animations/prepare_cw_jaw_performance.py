"""Prepare and verify CW's jaw-only performance using existing cooked facial shapes."""
import argparse
import copy
import hashlib
import json
from pathlib import Path

import numpy as np

from compile_facial_control import compile_control


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--compact-shapes', type=Path, required=True)
    parser.add_argument('--bake-report', type=Path, required=True)
    parser.add_argument('--source-poses', type=Path, required=True)
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
            raise ValueError('Facial input changed during preparation')
        return path

    def read(path):
        return json.loads(protect(path).read_text())

    manifest = read(args.catalog)
    compact = read(args.compact_shapes/'report.json')
    shapes_path = args.compact_shapes/'facial-shapes.json'
    payload = read(shapes_path)
    if not compact.get('complete') or protected[str(shapes_path.resolve())] != compact['sha256']:
        raise ValueError('Missing verified compact shapes')
    bake = read(args.bake_report)
    poses = read(args.source_poses/'report.json')
    if not bake.get('sources_unchanged') or not poses.get('sources_unchanged') or poses.get('tongue_fk'):
        raise ValueError('Require preserved sources and the original neutral mode')
    protect(Path(poses['source']))
    if protected[str(Path(poses['source']).resolve())] != poses['sha256']:
        raise ValueError('Original CW source changed')
    rigs = manifest['catalog']['animation_library']['faces']
    eve = next(rig for rig in rigs if rig['id'] == 'css.eve.face')
    commander = next(rig for rig in rigs if rig['id'] == 'css.commanderwhite.face')
    source = next(p for p in eve['performances'] if p['clip'] == 'css.eve.performance.example200')
    if len(source['curves']) != 1 or source['curves'][0]['morph'] != 'jaw_open':
        raise ValueError('This mapping only supports the verified jaw-only performance')
    if commander.get('performances'):
        raise ValueError('Expected the static-expression baseline')
    names = [f'CSSF_jnt_JawOpen_Q{i}' for i in range(1,5)]
    stored = np.zeros((4,len(payload['body_points']),3))
    for i,name in enumerate(names):
        if name not in commander['morphs']:
            raise ValueError('Missing existing CW jaw shape')
        shape = next(row for row in payload['morph_targets'] if row['name'] == name)
        for index,x,y,z in shape['deltas']:
            stored[i,index] = [x,y,z]
    gains = np.array([compact['shape_gains'][name] for name in names])
    times,weights,bound = compile_control(source['curves'][0]['keys'],stored,gains)
    performance = dict(clip=source['clip'],duration=source['duration'],curves=[
        dict(morph=name,keys=list(map(list,zip(times.tolist(),weights[:,i].tolist()))))
        for i,name in enumerate(names)])
    original = np.asarray(source['curves'][0]['keys'])
    if np.any(np.diff(original[:,1]) < 0):
        raise ValueError('The independent pose check requires the monotonic source jaw gesture')
    with np.load(protect(args.source_poses/'neutral.npz')) as data:
        neutral = {name:data[name].copy() for name in data.files}
    fitted = {index:i for i,index in enumerate(payload['point_indices'])}
    mappings = {}
    for name,points in neutral.items():
        offset = bake['offsets'][name]
        if offset >= bake['offsets']['Commander White Top Mesh']:
            offset += bake['topology_insertion_points']
        mappings[name] = [(i,fitted[offset+i]) for i in range(len(points)) if offset+i in fitted]
    filtered = np.where((np.linalg.norm(stored,axis=2)>.015)[:,:,None],stored,0)
    samples = []
    for row in poses['summary']:
        if row['control'] != 'facs_jnt_JawOpen':
            raise ValueError('Unexpected control in source validation poses')
        value = row['value']
        seconds = float(np.interp(value,original[:,1],original[:,0]))
        values = np.array([np.interp(seconds,times,weights[:,i]) for i in range(4)])
        actual = np.einsum('i,ijk->jk',values,filtered)
        errors = {}
        with np.load(protect(args.source_poses/f'facs_jnt_JawOpen-{value:g}.npz')) as data:
            for name,points in neutral.items():
                expected = data[name]-points
                expected[:,1] *= -1
                predicted = np.zeros_like(expected)
                for src,dst in mappings[name]:
                    predicted[src] = actual[dst]
                errors[name] = float(np.linalg.norm(expected-predicted,axis=1).max())
        samples.append(dict(value=value,seconds=seconds,weights=values.tolist(),errors_cm=errors))
    if len(samples) < 32:
        raise ValueError('Require at least 32 independent source control poses')
    maximum = max(v for row in samples for v in row['errors_cm'].values())
    if maximum > .015:
        raise ValueError('Mapped facial geometry differs from the source beyond 0.015 cm')
    candidate = copy.deepcopy(manifest)
    next(r for r in candidate['catalog']['animation_library']['faces']
         if r['id']=='css.commanderwhite.face')['performances'] = [performance]
    serialized = json.dumps(candidate,separators=(',',':'),allow_nan=False)+'\n'
    if len(serialized.encode()) > 256*1024:
        raise ValueError('Mapped performance exceeds the existing catalog size limit')
    for path in list(protected):
        protect(Path(path))
    args.output.mkdir(parents=True)
    outputs = {
        'performance.json': performance,
        'native-fixture.json': dict(morphs=names,performance=performance,samples=samples),
        'verification.json': dict(passed=True,inputs=protected,control='facs_jnt_JawOpen',
            source_performance=source,bound=bound,samples=samples,max_error_cm=maximum,
            limit_cm=.015,candidate_manifest_bytes=len(serialized.encode()),installed=False,
            scope='Quarter-sample jaw transfer against fresh source poses, with morph-filter simulation. Packaged/native playback and live appearance remain separate.'),
    }
    for name,value in outputs.items():
        (args.output/name).write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
    (args.output/'candidate-manifest.json').write_text(serialized)
    print('CW_JAW_PERFORMANCE',len(times),'keys per curve;',maximum,'cm source error')


if __name__ == '__main__':
    main()

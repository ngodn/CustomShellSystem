"""Compare cooked ACL runtime samples with raw UE poses using bounded chunks."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('decoded', type=Path)
    parser.add_argument('raw', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    decoded = json.loads((args.decoded/'decoded.json').read_text())
    raw_report = json.loads((args.raw/'result.json').read_text())
    if not decoded['complete'] or not raw_report['passed']:
        raise ValueError('Incomplete decoded or raw evidence')
    raw_by_id = {r['id']: Path(r['metadata']) for r in raw_report['clips']}
    if len(raw_by_id) != len(raw_report['clips']) or set(raw_by_id) != {r['id'] for r in decoded['results']}:
        raise ValueError('Raw and cooked inventory differ')
    results, inputs = [], {}
    limits = dict(position_cm=.01, quaternion_distance=.001, scale=.00001)
    for index, entry in enumerate(decoded['results']):
        directory = args.decoded/f'{index:02d}'
        manifest_path = directory/'absolute-tracks-full.json'
        cooked = json.loads(manifest_path.read_text())
        metadata = raw_by_id[entry['id']]
        raw = json.loads(metadata.read_text())
        frames, names = raw['frames'], raw['names']
        if (raw['component_type'] != 'float64-le' or raw['should_retarget'] is not False or
                raw['layout'] != 'frame,bone,qx-qy-qz-qw-tx-ty-tz-sx-sy-sz' or
                cooked['componentType'] != 'float32-le' or cooked['roundingPolicy'] != 'none' or
                cooked['layout'] != 'frame,track,component' or cooked['frames'] != frames or
                cooked['components'] != ['qx','qy','qz','qw','tx','ty','tz','sx','sy','sz'] or
                len(set(names)) != len(names) or len(set(cooked['names'])) != len(cooked['names']) or
                set(names) != set(cooked['names']) or
                abs(raw['rate_numerator']/raw['rate_denominator']-cooked['sampleRate']) > .001 or
                cooked['asset'] != raw['package']+'.'+raw['package'].rsplit('/',1)[1]):
            raise ValueError('Raw/cooked schema, identity, mapping or clock mismatch')
        binaries = [Path(raw['file']), directory/cooked['file']]
        for path, item, width in zip(binaries, (raw,cooked), (8,4)):
            if path.stat().st_size != frames*len(names)*10*width or item['bytes'] != path.stat().st_size or sha(path) != item['sha256']:
                raise ValueError('Invalid transform file size or checksum')
            inputs[str(path.resolve())] = item['sha256']
        for path in (metadata, manifest_path): inputs[str(path.resolve())] = sha(path)
        a = np.memmap(binaries[0], dtype='<f8', mode='r', shape=(frames,len(names),10))
        b = np.memmap(binaries[1], dtype='<f4', mode='r', shape=(frames,len(names),10))
        order = [cooked['names'].index(name) for name in names]
        maxima = dict.fromkeys(limits,0.)
        worst = {}
        for start in range(0,frames,64):
            x, y = np.asarray(a[start:start+64]), np.asarray(b[start:start+64,order],dtype=float)
            if not np.isfinite(x).all() or not np.isfinite(y).all() or any(
                np.max(np.abs(np.linalg.norm(v[:,:,:4],axis=2)-1)) >= .001 for v in (x,y)):
                raise ValueError('Nonfinite or non-unit cooked/raw transforms')
            errors = dict(position_cm=np.linalg.norm(x[:,:,4:7]-y[:,:,4:7],axis=2),
                quaternion_distance=np.minimum(np.linalg.norm(x[:,:,:4]-y[:,:,:4],axis=2),np.linalg.norm(x[:,:,:4]+y[:,:,:4],axis=2)),
                scale=np.linalg.norm(x[:,:,7:]-y[:,:,7:],axis=2))
            for key,error in errors.items():
                frame,bone = np.unravel_index(np.argmax(error),error.shape)
                value = float(error[frame,bone])
                if value > maxima[key]:
                    maxima[key],worst[key] = value,dict(frame=int(start+frame),bone=names[bone])
        passed = all(maxima[key] < limit for key,limit in limits.items())
        results.append(dict(id=entry['id'],frames=frames,bones=len(names),passed=passed,errors=maxima,worst=worst))
        del a,b
    args.output.write_text(json.dumps(dict(passed=all(r['passed'] for r in results),clips=results,
        limits=limits,inputs=inputs,rounding_policy='none',
        scope='Every source-frame timestamp: cooked ACL runtime samples versus raw UE poses. Includes float-clock and compression differences; does not validate gameplay.'),indent=2)+'\n')
    if not all(r['passed'] for r in results):
        raise ValueError('Cooked key differences exceed bounds; see report')


if __name__ == '__main__':
    main()

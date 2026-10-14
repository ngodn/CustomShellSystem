"""Require a cooked tangent bake to change only previously zero tangent corners."""
import argparse
from collections import Counter
import json
from pathlib import Path

import numpy as np

from check_import_cooked_mesh import read_audit, sha
from measure_cooked_tangent_basis import signed_bytes


def corners(path, inputs):
    audit = read_audit(path,'skin',inputs)
    metadata = path/'skin-surface.json'
    inputs[str(metadata)] = sha(metadata)
    info = json.loads(metadata.read_text())
    payload = path/info['file']['name']
    if sha(payload)!=info['file']['sha256'] or payload.stat().st_size!=info['file']['bytes']:
        raise ValueError('Surface data changed')
    inputs[str(payload)] = sha(payload)
    surface_type = np.dtype([('n','<f4',(4,)),('t','<f4',(4,)),
        ('uv','<f4',(info['uv_channels'],2)),('color','u1',(len(info['color_channels']),4))])
    surface = np.fromfile(payload,dtype=surface_type)
    weights = np.dtype([('bone','<u2'),('raw','<u2'),('weight','<f4')])
    vertex_type = np.dtype([('p','<f4',(3,)),('weights',weights,(audit['max_influences'],))])
    vertices = np.fromfile(path/'skin-vertices.bin',dtype=vertex_type)
    indices = np.fromfile(path/'skin-indices.bin',dtype='<u4')
    if surface_type.itemsize!=info['stride'] or vertex_type.itemsize!=audit['vertex_stride'] or len(surface)!=len(vertices) or len(vertices)!=audit['vertex_count']:
        raise ValueError('Unexpected stream layout')
    if not len(indices) or indices.max()>=len(vertices):
        raise ValueError('Invalid indices')
    key_type = np.dtype([('material','<i4'),('position','<f4',(3,)),('normal','<f4',(3,)),
        ('uv','<f4',(info['uv_channels'],2)),('color','u1',(len(info['color_channels']),4)),
        ('normal_w','<f4'),('tangent','<f4',(4,))])
    data = np.zeros(len(indices),dtype=key_type)
    data['material'] = -1
    for section in audit['sections']:
        start,count = section['FirstIndex'],section['NumFaces']*3
        if start<0 or start+count>len(indices) or np.any(data['material'][start:start+count]!=-1):
            raise ValueError('Invalid section ranges')
        data['material'][start:start+count] = section['MaterialIndex']
    if np.any(data['material']<0):
        raise ValueError('Uncovered corners')
    data['position'] = vertices['p'][indices]
    data['normal'] = surface['n'][indices,:3]
    data['normal_w'] = surface['n'][indices,3]
    data['tangent'] = surface['t'][indices]
    data['uv'] = surface['uv'][indices]
    data['color'] = surface['color'][indices]
    tangent = signed_bytes(surface['t'][:,:3].astype(float))
    if not np.isfinite(tangent).all():
        raise ValueError('Nonfinite tangent')
    zero = np.all(tangent[indices]==0,axis=1)
    # W encodes handedness and has no valid old value at a zero tangent.
    prefix = key_type.fields['normal_w'][1]
    valid = Counter(row.tobytes() for row in data[~zero])
    invalid = Counter(row.tobytes()[:prefix] for row in data[zero])
    return valid,invalid,prefix,audit['material_slots'],data,zero


def check_repaired_directions(before, zero, extra, prefix):
    expectations = {}
    for corner in np.flatnonzero(zero):
        start = int(corner)//3*3
        triangle = before[start:start+3]
        edge = triangle['position'][1:].astype(float)-triangle['position'][0].astype(float)
        uv = triangle['uv'][:,0].astype(float)
        d = uv[1:]-uv[0]
        determinant = np.linalg.det(d)
        if not np.isfinite(determinant) or determinant==0:
            raise ValueError('Zero corner has collapsed UVs')
        raw = (edge[0]*d[1,1]-edge[1]*d[0,1])/determinant
        normal = signed_bytes(before['normal'][corner].astype(float))
        normal /= np.linalg.norm(normal)
        tangent = raw-normal*np.dot(raw,normal)
        tangent /= np.linalg.norm(tangent)
        signs = signed_bytes(triangle['normal_w'][~zero[start:start+3]].astype(float))
        if not len(signs) or not np.all(signs==signs[0]) or abs(signs[0])!=1:
            raise ValueError('Neighbor signs do not agree')
        key = before[corner].tobytes()[:prefix]
        expectations.setdefault(key,[]).append((tangent,float(signs[0])))
    results = []
    for key,count in extra.items():
        row = np.frombuffer(key,dtype=before.dtype)[0]
        tangent = signed_bytes(row['tangent'][:3].astype(float))
        tangent /= np.linalg.norm(tangent)
        sign = float(signed_bytes(np.array([row['normal_w']],dtype=float))[0])
        candidates = expectations.get(key[:prefix],[])
        errors = [float(np.linalg.norm(tangent-t)) for t,s in candidates if s==sign]
        error = min(errors) if errors else None
        # Both input normal and output tangent are quantized to signed bytes.
        results.append(dict(corners=count,gradient_direction_error=error,binormal_sign=sign,
            passed=error is not None and error<.02))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('before','after','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs = {str(Path(__file__)):sha(Path(__file__))}
    for name in ('check_import_cooked_mesh.py','measure_cooked_tangent_basis.py'):
        p = Path(__file__).with_name(name)
        inputs[str(p)] = sha(p)
    old,old_zero,prefix,slots,old_data,zero = corners(args.before,inputs)
    new,new_zero,new_prefix,new_slots,_,_ = corners(args.after,inputs)
    if slots!=new_slots or prefix!=new_prefix:
        raise ValueError('Corner layout changed')
    missing,extra = old-new,new-old
    extra_locations = Counter()
    for key,count in extra.items():
        extra_locations[key[:prefix]] += count
    directions = check_repaired_directions(old_data,zero,extra,prefix)
    passed = not missing and not new_zero and extra_locations==old_zero and bool(directions) and all(r['passed'] for r in directions)
    changed = [p for p,h in inputs.items() if sha(Path(p))!=h]
    passed = passed and not changed
    result = dict(passed=passed,old_zero_corners=sum(old_zero.values()),
        new_zero_corners=sum(new_zero.values()),missing_valid_corners=sum(missing.values()),
        added_valid_corners=sum(extra.values()),repaired_corner_locations_match=extra_locations==old_zero,
        repaired_directions=directions,
        inputs=inputs,changed_inputs=changed,
        scope='Exact cooked position/material/all-UV/color/packed-normal/tangent correspondence per triangle-corner multiset. All previously valid tangent and handedness values must remain identical. Separate geometry, skin, morph, orthogonality and binding checks remain required.')
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('inputs','scope')}))
    if not passed:
        raise SystemExit(1)


if __name__=='__main__':
    main()

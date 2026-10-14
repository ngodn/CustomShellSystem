"""Compare remaining zero tangents with the same corners in installed Eve meshes.

Python 3.14 and NumPy. This records an existing limitation, not correct shading.
"""
import argparse
import json
from pathlib import Path

import numpy as np

from check_import_cooked_mesh import read_audit, sha
from measure_cooked_tangent_basis import signed_bytes


def load(surface_path, geometry_path, inputs):
    audit = read_audit(geometry_path, 'skin', inputs)
    surface_audit_path = surface_path/'skin-audit.json'
    inputs[str(surface_audit_path)] = sha(surface_audit_path)
    surface_audit = json.loads(surface_audit_path.read_text())
    for field in ('asset', 'vertex_count', 'files', 'sections', 'material_slots'):
        if surface_audit[field] != audit[field]:
            raise ValueError('Surface and geometry readbacks differ: '+field)
    metadata = surface_path/'skin-surface.json'
    inputs[str(metadata)] = sha(metadata)
    info = json.loads(metadata.read_text())
    payload = surface_path/info['file']['name']
    if sha(payload) != info['file']['sha256'] or payload.stat().st_size != info['file']['bytes']:
        raise ValueError('Surface payload changed')
    inputs[str(payload)] = sha(payload)
    dtype = np.dtype([('n','<f4',(4,)), ('t','<f4',(4,)),
                      ('uv','<f4',(info['uv_channels'],2)),
                      ('color','u1',(len(info['color_channels']),4))])
    data = np.fromfile(payload,dtype=dtype)
    weights = np.dtype([('bone','<u2'),('raw','<u2'),('weight','<f4')])
    vertex_type = np.dtype([('position','<f4',(3,)),('weights',weights,(audit['max_influences'],))])
    vertices = np.fromfile(geometry_path/'skin-vertices.bin',dtype=vertex_type)
    indices = np.fromfile(geometry_path/'skin-indices.bin',dtype='<u4').reshape(-1,3)
    if dtype.itemsize != info['stride'] or vertex_type.itemsize != audit['vertex_stride'] or len(data) != len(vertices) or len(data) != audit['vertex_count']:
        raise ValueError('Invalid stream layout')
    if not len(indices) or indices.max() >= len(vertices):
        raise ValueError('Invalid triangle index')
    materials = np.full(len(vertices),-1,dtype=np.int32)
    for s in audit['sections']:
        if s['FirstIndex'] % 3:
            raise ValueError('Misaligned section')
        begin = s['FirstIndex']//3
        touched = np.unique(indices[begin:begin+s['NumFaces']])
        if np.any((materials[touched] != -1) & (materials[touched] != s['MaterialIndex'])):
            raise ValueError('Shared vertex crosses material sections')
        materials[touched] = s['MaterialIndex']
    n = signed_bytes(data['n'][:,:3].astype(float))
    t = signed_bytes(data['t'][:,:3].astype(float))
    if not all(np.isfinite(x).all() for x in (n,t,vertices['position'],data['uv'])):
        raise ValueError('Nonfinite geometry or surface data')
    def key(i):
        return (int(materials[i]), *vertices['position'][i].tolist(),
                *data['uv'][i].ravel().tolist(), *n[i].tolist())
    return audit,vertices,indices,n,t,key


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('candidate','baseline-surface','baseline-geometry','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs = {str(Path(__file__)):sha(Path(__file__))}
    for name in ('check_import_cooked_mesh.py','measure_cooked_tangent_basis.py'):
        p = Path(__file__).with_name(name)
        inputs[str(p)] = sha(p)
    old = load(args.baseline_surface,args.baseline_geometry,inputs)
    new = load(args.candidate,args.candidate,inputs)
    bound = float(np.sqrt(3)/127+2e-5)
    old_zero = np.flatnonzero(np.all(old[4] == 0,axis=1))
    old_keys = {old[5](int(i)) for i in old_zero}
    new_zero = np.flatnonzero(np.all(new[4] == 0,axis=1))
    invalid_n = np.flatnonzero(np.abs(np.linalg.norm(new[3],axis=1)-1)>bound)
    invalid_t = np.flatnonzero(np.abs(np.linalg.norm(new[4],axis=1)-1)>bound)
    rows = []
    for i in new_zero:
        key = new[5](int(i))
        faces = np.flatnonzero(np.any(new[2]==i,axis=1))
        rows.append(dict(vertex=int(i),position=new[1]['position'][i].tolist(),
            material_slot=new[0]['material_slots'][key[0]],
            incident_triangles=faces.tolist(),exact_installed_corner=key in old_keys))
    matches = all(r['exact_installed_corner'] for r in rows)
    no_other_length_errors = len(invalid_n)==0 and np.array_equal(invalid_t,new_zero)
    changed = [p for p,h in inputs.items() if sha(Path(p)) != h]
    passed = matches and no_other_length_errors and not changed
    args.output.write_text(json.dumps(dict(passed=passed,baseline_asset=old[0]['asset'],
        candidate_asset=new[0]['asset'],baseline_zero_tangents=len(old_zero),
        candidate_zero_tangents=len(new_zero),rows=rows,
        no_other_length_errors=no_other_length_errors,inputs=inputs,changed_inputs=changed,
        scope='Exact position/material/all-UV/packed-normal correspondence of remaining zero tangents to installed zero tangents. Does not repair or accept their shading. All other candidate tangent/normal lengths must be within packing tolerance.'),indent=2)+'\n')
    print(json.dumps(dict(passed=passed,baseline_zero_tangents=len(old_zero),candidate_zero_tangents=len(new_zero),rows=rows)))
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

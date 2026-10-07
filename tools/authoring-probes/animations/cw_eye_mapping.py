"""Measure source eye rotations and solve CW's two-control gaze mapping."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def rigid_rotation(points, moved):
    a = np.asarray(points,dtype=float)
    b = np.asarray(moved,dtype=float)
    if (a.shape!=b.shape or a.ndim!=2 or a.shape[1]!=3 or len(a)<4 or
            not np.isfinite(a).all() or not np.isfinite(b).all()):
        raise ValueError('Expected matching three-dimensional eye vertices')
    a = a-a.mean(axis=0)
    b = b-b.mean(axis=0)
    u,s,vt = np.linalg.svd(a.T@b)
    if s[-1]<=s[0]*1e-8:
        raise ValueError('Degenerate eye geometry')
    rotation = u@vt
    if np.linalg.det(rotation)<0:
        raise ValueError('Eye fit would reflect the source')
    return rotation,float(np.linalg.norm(a@rotation-b,axis=1).max())


def scaled_rotation(rotation,value):
    """Return a row-vector rotation and its derivative with respect to value."""
    angle = float(np.arccos(np.clip((np.trace(rotation)-1)/2,-1,1)))
    if not 1e-6<angle<np.pi-1e-6 or not np.isfinite(value):
        raise ValueError('Invalid calibration rotation or control value')
    skew = (rotation-rotation.T)/(2*np.sin(angle))
    square = skew@skew
    theta = value*angle
    result = np.eye(3)+np.sin(theta)*skew+(1-np.cos(theta))*square
    derivative = angle*(np.cos(theta)*skew+np.sin(theta)*square)
    return result,derivative


def gaze(axes,order,values):
    a,da = scaled_rotation(axes[0],values[0])
    b,db = scaled_rotation(axes[1],values[1])
    front = np.array([0.,-1.,0.])
    if order=='horizontal-vertical':
        return front@a@b,np.stack((front@da@b,front@a@db),axis=1)
    if order=='vertical-horizontal':
        return front@b@a,np.stack((front@b@da,front@db@a),axis=1)
    raise ValueError('Unknown eye control order')


def solve_gaze(axes,order,direction):
    direction = np.asarray(direction,dtype=float)
    if direction.shape!=(3,) or not np.isfinite(direction).all() or abs(np.linalg.norm(direction)-1)>1e-6:
        raise ValueError('Expected a finite unit gaze direction')
    values = np.zeros(2)
    for _ in range(16):
        current,derivative = gaze(axes,order,values)
        error = current-direction
        if np.linalg.norm(error)<1e-9:
            break
        step = np.linalg.lstsq(derivative,error,rcond=1e-10)[0]
        values -= step
        if not np.isfinite(values).all() or np.max(np.abs(values))>4:
            raise ValueError('Gaze solve exceeded the explicit source-control range')
    if np.linalg.norm(gaze(axes,order,values)[0]-direction)>1e-7:
        raise ValueError('Gaze solve did not converge')
    return values


def blended_direction(rotations,weights):
    matrix = np.eye(3)+sum(float(w)*(r-np.eye(3)) for r,w in zip(rotations,weights,strict=True))
    u,s,vt = np.linalg.svd(matrix)
    if s[-1]<1e-5 or np.linalg.det(u@vt)<0:
        raise ValueError('Source eye blend has no stable proper rotation')
    return np.array([0.,-1.,0.])@u@vt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--eye-samples', default='cw-gaze-range-v2')
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
    source = work/'author-head-source-v1/head-morphs-v2'
    head = read(source/'morph-audit.json')
    for row in head['files']:
        path = protect(source/row['name'])
        if protected[str(path)]!=row['sha256']:
            raise ValueError('Audited head export changed')
    vertices = np.fromfile(source/'morph-vertices.bin','<f4').reshape(-1,5)[:,[1,0,2]].astype(float)
    section = head['sections'][2]
    if 'EyeRefractive' not in head['materials'][section['MaterialIndex']]['name']:
        raise ValueError('Expected the audited eye surface')
    eyes = np.arange(section['BaseVertexIndex'],section['BaseVertexIndex']+section['NumVertices'])
    blob = (source/'morph-deltas.bin').read_bytes()
    dtype = np.dtype([('index','<u4'),('position','<f4',(3,)),('normal','<f4',(3,))])
    source_rotations = {}
    for side,sign in [('L',1),('R',-1)]:
        for direction in ('left','right','up','down'):
            name = f'eye_{direction}_{side}'
            row = next(m for m in head['morphs'] if m['name']==name)
            values = np.frombuffer(blob,dtype,row['count'],row['offset_bytes'])
            delta = np.zeros_like(vertices)
            delta[values['index']] = values['position'][:,[1,0,2]]
            selected = eyes[(vertices[eyes,0]*sign>0)&(np.linalg.norm(delta[eyes],axis=1)>.0001)]
            rotation,error = rigid_rotation(vertices[selected],vertices[selected]+delta[selected])
            if error>.0002 or len(selected)<700:
                raise ValueError('Source eye geometry is not the expected rigid rotation: '+name)
            source_rotations[name] = dict(rotation=rotation.tolist(),error_cm=error,vertices=len(selected),
                angle_degrees=float(np.degrees(np.arccos(np.clip((np.trace(rotation)-1)/2,-1,1)))))
    regions = read(work/'cw-face-region-audit-v1/regions.json')['cw']
    protect(Path(regions['source']))
    if protected[str(Path(regions['source']).resolve())]!=regions['sha256']:
        raise ValueError('CW source changed')
    pupil_ids = np.asarray(regions['regions']['Pupils-2']['indices'])
    part = 'Commander White 8.1 Mesh'

    def points(folder,label):
        with np.load(protect(work/folder/'samples'/(label+'.npz'))) as data:
            return data[part].copy()

    neutral = points(args.eye_samples,'neutral')
    signed = read(work/args.eye_samples/'samples/report.json')
    if (not signed.get('sources_unchanged') or not signed.get('eye_gaze_range') or
            read(work/args.eye_samples/'exit.json')['exit_code']!=0):
        raise ValueError('Signed source sampling is incomplete')
    calibrations = {}
    for side,sign in [('Left',1),('Right',-1)]:
        ids = pupil_ids[neutral[pupil_ids,0]*sign>0]
        controls = [f'facs_ctrl_EyeLookSide-Side{side}',f'facs_ctrl_EyeLookUp-Down{side}']
        axes = []
        errors = []
        for control in controls:
            moved = points(args.eye_samples,control+'-1')
            rotation,error = rigid_rotation(neutral[ids],moved[ids])
            if error>.0002:
                raise ValueError('CW eye control is not a rigid rotation')
            axes.append(rotation)
            errors.append(error)
        cases = []
        for row in signed['summary']:
            if row['control'] not in controls:
                continue
            moved = points(args.eye_samples,f'{row["control"]}-{row["value"]:g}')
            rotation,error = rigid_rotation(neutral[ids],moved[ids])
            expected,_ = scaled_rotation(axes[controls.index(row['control'])],row['value'])
            difference = float(np.max(np.abs(rotation-expected)))
            if error>.0002 or difference>2e-5:
                raise ValueError(f'Signed CW rotation differs: {row["control"]} {row["value"]}, '
                                 f'rigid error {error} cm, rotation difference {difference}, '
                                 f'actual {rotation.tolist()}, expected {expected.tolist()}')
            cases.append(dict(control=row['control'],value=row['value'],error_cm=error,rotation_error=difference))
        orders = {'horizontal-vertical':0.,'vertical-horizontal':0.}
        for row in signed['combinations']:
            if not all(c in row['controls'] for c in controls):
                continue
            moved = points(args.eye_samples,row['label'])
            actual,error = rigid_rotation(neutral[ids],moved[ids])
            if error>.0002:
                raise ValueError('Combined CW eye controls are not rigid')
            a,_ = scaled_rotation(axes[0],row['controls'][controls[0]])
            b,_ = scaled_rotation(axes[1],row['controls'][controls[1]])
            for order,expected in [('horizontal-vertical',a@b),('vertical-horizontal',b@a)]:
                orders[order] = max(orders[order],float(np.max(np.abs(actual-expected))))
        order = min(orders,key=orders.get)
        if orders[order]>2e-5 or max(orders.values())<.01:
            raise ValueError('Eye control order was not distinguished by the source samples')
        calibrations[side] = dict(controls=controls,axes=[a.tolist() for a in axes],order=order,
            order_errors=orders,source_fit_errors_cm=errors,signed_cases=cases)
    for path in list(protected):
        protect(Path(path))
    result = dict(passed=True,protected=protected,source_rotations=source_rotations,
        commanderwhite=calibrations,
        scope='Measured eye rotations and control composition, not facial performance or eyelid acceptance.')
    args.output.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    print('CW_EYE_MAPPING_PASS',[(k,v['order']) for k,v in calibrations.items()],flush=True)


if __name__=='__main__':
    main()

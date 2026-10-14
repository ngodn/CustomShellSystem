"""Reproduce isolated zero-tangent triangles using UE's installed MikkTSpace.

Python 3.14/NumPy and the system C17 compiler. No mesh or library is modified.
"""
import argparse
import json
from pathlib import Path
import subprocess

import numpy as np

from check_import_cooked_mesh import sha
from trace_eve_zero_tangents import load

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source','readback','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to((ROOT/'work').resolve()):
        raise ValueError('Use the private work tree')
    out.mkdir(exist_ok=False)
    library = ROOT.parent/'CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/Source/ThirdParty/MikkTSpace'
    archive = library/'lib/Unix/x86_64-unknown-linux-gnu/libMikkTSpace.a'
    native = Path(__file__).with_name('mikk_triangle_probe.c')
    files = [Path(__file__),native,archive,library/'inc/mikktspace.h',args.source,
             Path(__file__).with_name('trace_eve_zero_tangents.py'),
             Path(__file__).with_name('check_import_cooked_mesh.py'),
             Path(__file__).with_name('measure_cooked_tangent_basis.py')]
    inputs = {str(p):sha(p) for p in files}
    data = load(args.readback,args.readback,inputs)
    zero = np.flatnonzero(np.all(data[4]==0,axis=1))
    if len(zero)!=1:
        raise ValueError('Expected one remaining zero tangent')
    affected = np.flatnonzero(np.any(data[2]==zero[0],axis=1))
    targets = {frozenset(tuple(p) for p in data[1]['position'][data[2][i]]) for i in affected}
    materials = {data[5](int(i))[0] for i in zero}
    source = json.loads(args.source.read_text())
    points = np.asarray(source['points'],dtype=np.float32)
    matches = []
    for i,face in enumerate(source['faces']):
        if face[3] in materials and frozenset(tuple(points[source['wedges'][w][0]]) for w in face[:3]) in targets:
            matches.append((i,face))
    if not matches:
        raise ValueError('No source triangle matches the cooked position triplet')
    command = ['cc','-std=c17','-O2','-I'+str(library/'inc'),str(native),str(archive),'-lm','-o',str(out/'probe')]
    compile_result = subprocess.run(command,capture_output=True,text=True)
    (out/'compile.json').write_text(json.dumps(dict(command=command,returncode=compile_result.returncode,
        stdout=compile_result.stdout,stderr=compile_result.stderr,
        compiler=subprocess.check_output(['cc','--version'],text=True).splitlines()[0]),indent=2)+'\n')
    compile_result.check_returncode()
    inputs[str(out/'probe')] = sha(out/'probe')
    results = []
    for index,face in matches:
        corners = []
        for w in face[:3]:
            wedge = source['wedges'][w]
            corners.append([*source['points'][wedge[0]],*source['normals'][w],*wedge[1:3]])
        text = '\n'.join(' '.join(str(float(v)) for v in row) for row in corners)+'\n'
        input_path = out/('triangle-'+str(index)+'.txt')
        input_path.write_text(text)
        inputs[str(input_path)] = sha(input_path)
        row = dict(source_face=index,material=source['materials'][face[3]],corners=corners,probes=[])
        for ignore in (0,1):
            command = [str(out/'probe'),str(ignore)]
            result = subprocess.run(command,input=text,capture_output=True,text=True)
            parsed = [[float(v) for v in line.split()] for line in result.stdout.splitlines()]
            if result.returncode or len(parsed)!=3 or any(len(v)!=5 for v in parsed):
                raise ValueError('Native probe failed: '+str(result))
            row['probes'].append(dict(command=command,returncode=result.returncode,
                stdout=result.stdout,stderr=result.stderr,
                zero_tangent_corners=[int(v[0]) for v in parsed if v[1:4]==[0,0,0]]))
        results.append(row)
    changed = [p for p,h in inputs.items() if sha(Path(p))!=h]
    if changed:
        raise ValueError('Probe input changed: '+str(changed))
    reproduced = any(p['zero_tangent_corners'] for r in results for p in r['probes'])
    report = dict(zero_reproduced=reproduced,cooked_zero_vertex=int(zero[0]),
        cooked_position=data[1]['position'][zero[0]].tolist(),results=results,inputs=inputs,
        scope='Each matching source triangle evaluated independently through the installed UE MikkTSpace library. Includes opposite windings if present. No whole-mesh equivalence or shading acceptance claimed.')
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(zero_reproduced=reproduced,source_triangles=len(results),position=report['cooked_position'])))


if __name__ == '__main__':
    main()

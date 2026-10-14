"""Read and compare one installed normal mesh with its retained import payload.

Python 3.14. Uses the installed-inventory mount and existing MeshExport audits.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('inventory', 'source', 'output', 'dotnet', 'mappings'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--asset', required=True, help='Mesh package path from the inventory')
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text())
    if args.asset not in inventory['meshes']:
        raise ValueError('Mesh is absent from inventory')
    if not any(v['meshes']['normal'] == args.asset for v in inventory['variants']):
        raise ValueError('This comparison expects a normal mesh import')
    out = args.output.resolve()
    if not out.is_relative_to((ROOT/'work').resolve()):
        raise ValueError('Output must be under CSS/work')
    out.mkdir(exist_ok=False)
    reader = ROOT/'tools/MeshExport/bin/Release/net10.0/MeshExport.dll'
    checker = ROOT/'tools/diagnostics/check_import_cooked_mesh.py'
    paths = (args.inventory, args.source, args.dotnet, args.mappings, reader, checker, Path(__file__))
    inputs = {str(p.resolve()): sha(p) for p in paths}
    for path, digest in inventory['protected'].items():
        if sha(Path(path)) != digest:
            raise ValueError('Installed input no longer matches inventory: '+path)
        inputs[path] = digest
    (out/'inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')
    object_path = args.asset+'.'+args.asset.rsplit('/', 1)[1]
    results = {}
    for mode in ('skin', 'morph', 'comparison'):
        overrides = {}
        if mode == 'comparison':
            command = [sys.executable, '-B', str(checker), '--source', str(args.source.resolve()),
                       '--skin', str(out/'skin'), '--morph', str(out/'morph'),
                       '--output', str(out/'comparison.json')]
        else:
            command = [str(args.dotnet), str(reader), str(args.inventory.parent/'containers'),
                       str(args.mappings), object_path, str(out/mode)]
            overrides['CSS_'+mode.upper()+'_AUDIT'] = '1'
        (out/(mode+'-command.json')).write_text(json.dumps(
            dict(command=command, environment=overrides), indent=2)+'\n')
        with (out/(mode+'.log')).open('w') as log:
            result = subprocess.run(command, env={**os.environ, **overrides},
                                    stdout=log, stderr=subprocess.STDOUT)
        results[mode] = result.returncode
        (out/(mode+'-exit.json')).write_text(json.dumps(dict(returncode=result.returncode))+'\n')
        print(f'{args.asset}: {mode} exit {result.returncode}', flush=True)
        if result.returncode:
            break
    changed = [p for p, digest in inputs.items() if sha(Path(p)) != digest]
    report = dict(passed=not changed and results == dict(skin=0, morph=0, comparison=0),
                  commands=results, changed_inputs=changed,
                  scope='Import-to-installed LOD0 fields checked by check_import_cooked_mesh.py. '
                        'No source Blender, facial, shading, cloth simulation or gameplay acceptance.')
    (out/'verified.json').write_text(json.dumps(report, indent=2)+'\n')
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

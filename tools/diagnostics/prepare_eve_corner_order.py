"""Reorder an accepted Eve interchange into triangle corners for a private import.

Reuses the correction proven by Eve's prepare_peony_corner_order.py. Python 3.14.
"""
import argparse
import json
from pathlib import Path

from check_import_cooked_mesh import sha


def reorder(source, mesh_package, skeleton_package):
    result = dict(source, mesh_package=mesh_package, skeleton_package=skeleton_package)
    order = [w for face in source['faces'] for w in face[:3]]
    for key in ('wedges', 'normals', 'colors'):
        result[key] = [source[key][w] for w in order]
    result['faces'] = [[3*i, 3*i+1, 3*i+2, face[3]] for i, face in enumerate(source['faces'])]
    return result


def verify(source, result):
    changed = {'mesh_package', 'skeleton_package', 'wedges', 'normals', 'colors', 'faces'}
    if source.keys() != result.keys() or any(source[k] != result[k] for k in source.keys() - changed):
        raise ValueError('Changed non-corner source fields')
    for old, new in zip(source['faces'], result['faces'], strict=True):
        if old[3] != new[3]:
            raise ValueError('Changed material section')
        for a, b in zip(old[:3], new[:3], strict=True):
            if any(source[key][a] != result[key][b] for key in ('wedges', 'normals', 'colors')):
                raise ValueError('Changed triangle corner attribute')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--name', required=True)
    args = parser.parse_args()
    if not args.name.isalnum():
        raise ValueError('Diagnostic name must be alphanumeric')
    report = args.output.with_suffix('.receipt.json')
    if args.output.exists() or report.exists():
        raise FileExistsError('Use a new candidate path')
    before = sha(args.source)
    source = json.loads(args.source.read_text())
    base = '/Game/CSSAuthoring/EveSurface20261014/'
    result = reorder(source, base+'SK_'+args.name, base+'SKEL_'+args.name)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, separators=(',', ':'), allow_nan=False))
    reloaded = json.loads(args.output.read_text())
    verify(source, reloaded)
    if sha(args.source) != before:
        raise ValueError('Accepted source changed')
    report.write_text(json.dumps(dict(passed=True, source=str(args.source.resolve()), source_sha256=before,
        output=str(args.output.resolve()), output_sha256=sha(args.output), recipe_sha256=sha(Path(__file__)),
        mesh_package=result['mesh_package'], skeleton_package=result['skeleton_package'],
        triangles=len(result['faces']), original_wedges=len(source['wedges']), wedges=len(result['wedges']),
        preserved='Every triangle corner retains point index, all UVs, normal and color; all other fields except diagnostic package names are unchanged.',
        scope='Import candidate only. Editor/cook, surface basis, bindings, cloth, facial and gameplay checks remain.'), indent=2)+'\n')
    print(report)


if __name__ == '__main__':
    main()

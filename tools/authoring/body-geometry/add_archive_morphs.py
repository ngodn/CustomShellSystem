#!/usr/bin/env python3
"""Write a fixture mesh JSON: an export plus morphs from a uuepbs delta archive.

    add_archive_morphs.py EXPORT.mesh.json DELTAS.npz OUTPUT.mesh.json
        --package /Game/CSSAuthoring/<outfit>/SK_... --skeleton /Game/CSSAuthoring/<outfit>/SKEL_...
        --morph NAME [--morph NAME ...]

For editor checks only (CSSImportMesh into a private stage), never a package
export: the archive carries body deltas only, so garments do not follow. The
archive's `used` row map and its neutral points are checked against the
export's body part (to 1e-4 cm) before anything is written.
"""
import argparse
import json
from pathlib import Path

import numpy as np

TO_UE = np.array([100., -100., 100.])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('export', type=Path)
    parser.add_argument('deltas', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--package', required=True)
    parser.add_argument('--skeleton', required=True)
    parser.add_argument('--morph', action='append', required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    mesh = json.loads(args.export.read_text())
    audit = json.loads(args.export.with_suffix('.audit.json').read_text())
    if audit['parts'][0]['name'] != 'Eve Body':
        raise ValueError('Export does not start with the original body')
    archive = np.load(args.deltas)
    used = archive['used']
    body = audit['parts'][0]['points']
    if len(used) != body:
        raise ValueError('Delta archive does not cover the exported body')
    error = float(np.linalg.norm(archive['points_world'][used]*TO_UE-np.asarray(mesh['points'][:body]), axis=1).max())
    if error > 1e-4:
        raise ValueError(f'Delta archive neutral differs from the export by {error} cm')
    present = {m['name'] for m in mesh['morph_targets']}
    added = {}
    for name in args.morph:
        if name in present or name not in archive.files:
            raise ValueError(f'{name}: already in the export or missing from the archive')
        delta = archive[name][used].astype(np.float64)*TO_UE
        moved = np.flatnonzero(np.linalg.norm(delta, axis=1) > 1e-6)
        mesh['morph_targets'].append(dict(name=name, deltas=[[int(i), *map(float, delta[i])] for i in moved]))
        added[name] = len(moved)
    mesh['mesh_package'], mesh['skeleton_package'] = args.package, args.skeleton
    args.output.write_text(json.dumps(mesh, separators=(',', ':'))+'\n')
    print(json.dumps(dict(output=str(args.output), added=added, neutral_error_cm=error,
                          morphs=[m['name'] for m in mesh['morph_targets']])))


if __name__ == '__main__':
    main()

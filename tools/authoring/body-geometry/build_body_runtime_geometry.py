#!/usr/bin/env python3
"""Pack measured morph geometry for the native core and emit parity fixtures.

Copied from the SeduXtress authoring library (build_body_runtime_geometry.py,
which stays unchanged and six-axis) and generalised: the solver axes come from
the controls recipe (`shape` controls with `solver_axis`, 6 to 8 of them), and
the packed model records them.

    build_body_runtime_geometry.py --recipe RECIPE --layout LAYOUT \\
        --ellipsoids ELLIPSOIDS --output MODEL [--fixtures FIXTURES]

RECIPE is a customize block or a package manifest. LAYOUT and ELLIPSOIDS are
the measured coefficient files (body-morph-layout-*.json and
body-contact-ellipsoids-*.json, from the editor fits or from
measure_body_geometry_axes.py); their `morphs` must equal the recipe's axes in
order, and every region must carry all 2^n ordered corners.

The model is schema 2: {"schema": 2, "morphs": [n axis names], "corners": 2^n,
"helpers": [...]}, the string cooked into CSSBodyGeometryJson. Schema 1 (the
six-axis model already cooked) has no "corners" and stays readable by the core.
The packed string must fit the 262144-character bound the AnimBP authoring step
and the native reader both enforce.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from body_geometry_axes import NAMES, PEERS, contact_inputs, layout_inputs, read_recipe, _inverse

SCHEMA = 2
STRING_BOUND = 262144
INTERIOR_CASES = 64


def pack(axes, measured, envelopes):
    count = len(axes)
    if (measured['morphs'] != axes or envelopes['morphs'] != axes or
            [h['bone'] for h in measured['helpers']] != NAMES or
            [r['name'] for r in envelopes['regions']] != NAMES):
        raise ValueError('Measured geometry identity differs from the recipe axes')
    helpers = []
    for index, peer in enumerate(PEERS):
        source, region = measured['helpers'][index], envelopes['regions'][peer]
        if (len(source['offset_deltas_cm']) != count or len(source['moment_linear_cm2']) != count or
                any(len(row) != count for row in source['moment_quadratic_cm2'])):
            raise ValueError('Coefficient rows differ from the axis count')
        reference = region['reference_parent_matrix']
        inverse = _inverse(reference)
        corners = region['morph_ellipsoids']
        if [c['mask'] for c in corners] != list(range(1 << count)):
            raise ValueError('Contact corner order differs')
        helper = {key: source[key] for key in ('bone', 'offset_cm', 'offset_deltas_cm',
                    'moment_cm2', 'moment_linear_cm2', 'moment_quadratic_cm2')}
        helper.update(contact_bone=region['parent'],
            contact_centers_cm=[inverse([c['center_cm'][i]-reference[i][3] for i in range(3)]) for c in corners],
            contact_scales=[c['scale'] for c in corners],
            contact_axes_cm=[inverse([region['shape_basis_cm'][i][j] for i in range(3)]) for j in range(3)])
        helpers.append(helper)
    return dict(schema=SCHEMA, morphs=list(axes), corners=1 << count, helpers=helpers)


def fixtures(axes, measured, envelopes):
    """Every corner plus seeded interiors, with values from the reference converters."""
    count = len(axes)
    rng = random.Random(379)
    f32 = lambda value: struct.unpack('<f', struct.pack('<f', value))[0]
    combinations = [[float((mask >> i) & 1) for i in range(count)] for mask in range(1 << count)]
    combinations += [[f32(rng.random()) for _ in range(count)] for _ in range(INTERIOR_CASES)]
    cases = []
    for values in combinations:
        expected = dict(**layout_inputs(measured, values), **contact_inputs(envelopes, values))
        expected['Moments'] = [f32(v) for v in expected['Moments']]
        cases.append(dict(morphs=values, expected=expected))
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--recipe', type=Path, required=True)
    parser.add_argument('--layout', type=Path, required=True)
    parser.add_argument('--ellipsoids', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path)
    args = parser.parse_args()
    for path in (args.output, args.fixtures):
        if path and path.exists():
            raise FileExistsError(f'Preserve existing native geometry evidence: {path}')
    axes = read_recipe(args.recipe)
    measured = json.loads(args.layout.read_text())
    envelopes = json.loads(args.ellipsoids.read_text())
    model = pack(axes, measured, envelopes)
    packed = json.dumps(model, separators=(',', ':'), allow_nan=False)
    if len(packed) > STRING_BOUND:
        raise ValueError(f'Body model is {len(packed)} characters; the AnimBP and core take {STRING_BOUND}')
    args.output.write_text(packed + '\n')
    report = dict(model=str(args.output), axes=axes, corners=model['corners'], bytes=len(packed),
                  bound=STRING_BOUND, sha256=hashlib.sha256(args.output.read_bytes()).hexdigest())
    if args.fixtures:
        cases = fixtures(axes, measured, envelopes)
        args.fixtures.write_text(json.dumps(dict(model_sha256=report['sha256'], axes=axes, cases=cases,
            scope='Native parity with the reference geometry converters. No live acceptance.'),
            indent=2, allow_nan=False) + '\n')
        report['fixtures'] = len(cases)
    print(json.dumps(report))


if __name__ == '__main__':
    main()

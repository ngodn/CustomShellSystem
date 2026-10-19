"""Body geometry with a recipe-chosen set of solver axes (6 to 8 morphs).

The measured body model (center, effective inertia and contact ellipsoids per
helper) is multilinear in its morph axes: centers are affine, inertia is
quadratic and the contact ellipsoids interpolate over all 2^n morph corners.
The six-axis model fixed the axes in code. Here they come from the outfit's
controls recipe: a `shape` control with `"solver_axis": i` is axis i, and the
indices must run 0..n-1 with 6 <= n <= 8.

The evaluators are n-axis copies of the SeduXtress authoring library's
body_morph_layout.inputs and body_contact_morph_inputs.inputs (unchanged
there); at n = 6 they compute the same expressions in the same order.
"""
import json
import math
from pathlib import Path

MINIMUM_AXES = 6
MAXIMUM_AXES = 8
NAMES = ['brust001', 'brust002', 'butt001', 'butt002', 'thigh_twist_02_l', 'thigh_twist_02_r', 'belly']
# Contact peers: each helper is held off its opposing region (waist has none).
PEERS = (1, 0, 3, 2, 5, 4, 6)


def _customize_blocks(recipe):
    """Every customize block in a package manifest, or the recipe itself."""
    if 'controls' in recipe:
        return [('customize', recipe)]
    blocks = []
    for outfit in recipe.get('catalog', {}).get('outfits', []):
        for variant in outfit.get('variants', []):
            if 'customize' in variant:
                blocks.append((f"{outfit.get('id', '?')}/{variant.get('id', '?')}", variant['customize']))
    if not blocks:
        raise ValueError('Recipe has no customize block')
    return blocks


def recipe_axes(recipe):
    """The solver axes a recipe declares, in axis order.

    Accepts a customize block or a package manifest. Every block that declares
    axes must declare the same list; blocks that declare none are skipped (a
    variant without the body). Raises when no block declares axes.
    """
    found = {}
    for where, block in _customize_blocks(recipe):
        axes = {}
        for control in block.get('controls', []):
            if 'solver_axis' not in control:
                continue
            index = control['solver_axis']
            if control.get('kind') != 'shape':
                raise ValueError(f'{where}: solver_axis on a {control.get("kind")} control: {control.get("id")}')
            if not isinstance(index, int) or isinstance(index, bool) or not 0 <= index < MAXIMUM_AXES:
                raise ValueError(f'{where}: solver_axis must be an integer 0..{MAXIMUM_AXES-1}: {control.get("id")}')
            if index in axes:
                raise ValueError(f'{where}: solver_axis {index} declared twice')
            axes[index] = control['morph']
        if not axes:
            continue
        if sorted(axes) != list(range(len(axes))):
            raise ValueError(f'{where}: solver axes must be numbered 0..n-1 without gaps')
        ordered = [axes[i] for i in range(len(axes))]
        if not MINIMUM_AXES <= len(ordered) <= MAXIMUM_AXES:
            raise ValueError(f'{where}: {len(ordered)} solver axes; the model takes {MINIMUM_AXES} to {MAXIMUM_AXES}')
        if len(set(ordered)) != len(ordered):
            raise ValueError(f'{where}: a morph is listed on two solver axes')
        found[where] = ordered
    if not found:
        raise ValueError('No shape control declares solver_axis')
    lists = {tuple(v) for v in found.values()}
    if len(lists) != 1:
        raise ValueError('Variants declare different solver axes: ' + json.dumps(found))
    return list(lists.pop())


def read_recipe(path):
    return recipe_axes(json.loads(Path(path).read_text()))


def check_values(morphs, count):
    if len(morphs) != count or not all(math.isfinite(v) and 0 <= v <= 1 for v in morphs):
        raise ValueError(f'Expected {count} finite body morph values in [0,1]')


def corner_weights(morphs):
    """Multilinear weight of every corner; corner bit i is axis i."""
    return [math.prod(v if mask & (1 << i) else 1-v for i, v in enumerate(morphs))
            for mask in range(1 << len(morphs))]


def layout_inputs(data, morphs):
    """Offsets and moments of the seven helpers (body_morph_layout.inputs, n axes)."""
    count = len(data['morphs'])
    check_values(morphs, count)
    if len(data['helpers']) != 7:
        raise ValueError('Expected seven measured body helpers')
    offsets, moments = [], []
    for helper in data['helpers']:
        offsets.append([helper['offset_cm'][j] + sum(morphs[i]*helper['offset_deltas_cm'][i][j]
                                                   for i in range(count)) for j in range(3)])
        moment = helper['moment_cm2'] + sum(morphs[i]*helper['moment_linear_cm2'][i] for i in range(count))
        moment += sum(morphs[i]*morphs[j]*helper['moment_quadratic_cm2'][i][j]
                      for i in range(count) for j in range(count))
        if not math.isfinite(moment) or moment <= 0:
            raise ValueError('Non-positive or non-finite body inertia')
        moments.append(moment)
    if not all(math.isfinite(v) for offset in offsets for v in offset):
        raise ValueError('Non-finite body center')
    return dict(Offsets=offsets, Moments=moments)


def _inverse(reference):
    dot = lambda a, b: sum(x*y for x, y in zip(a, b))
    cross = lambda a, b: [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
    columns = [[reference[i][j] for i in range(3)] for j in range(3)]
    cofactors = [cross(columns[1], columns[2]), cross(columns[2], columns[0]), cross(columns[0], columns[1])]
    determinant = dot(columns[0], cofactors[0])
    if not math.isfinite(determinant) or abs(determinant) < 1e-8:
        raise ValueError('Singular contact parent')
    return lambda v: [dot(c, v)/determinant for c in cofactors]


def contact_inputs(data, morphs):
    """Parent-local contact centers and axes (body_contact_morph_inputs.inputs, n axes)."""
    count = len(data['morphs'])
    check_values(morphs, count)
    regions = data['regions']
    if len(regions) != 7:
        raise ValueError('Expected seven body regions')
    weights = corner_weights(morphs)
    result = {name: [] for name in ('ContactCenters', 'ContactAxesX', 'ContactAxesY', 'ContactAxesZ')}
    for peer in PEERS:
        region = regions[peer]
        corners = region['morph_ellipsoids']
        if [c['mask'] for c in corners] != list(range(1 << count)):
            raise ValueError('Expected all ordered morph corners')
        center = [sum(w*c['center_cm'][j] for w, c in zip(weights, corners)) for j in range(3)]
        scale = sum(w*c['scale'] for w, c in zip(weights, corners))
        if scale <= 0:
            raise ValueError('Invalid reference contact geometry')
        reference = region['reference_parent_matrix']
        inverse = _inverse(reference)
        result['ContactCenters'].append(inverse([center[i]-reference[i][3] for i in range(3)]))
        for j, axis in enumerate('XYZ'):
            result['ContactAxes'+axis].append(inverse([region['shape_basis_cm'][i][j]*scale for i in range(3)]))
    if not all(math.isfinite(v) for values in result.values() for point in values for v in point):
        raise ValueError('Non-finite contact inputs')
    return result

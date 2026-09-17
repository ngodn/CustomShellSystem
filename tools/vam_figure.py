"""Figure definitions: named body shapes built by stacking VaM morphs.

A figure is a weighted stack of morphs from the library, evaluated against the
Genesis 2 Female base. The stack is the unit an author works in, so "OMG!" is a
recipe rather than a separately sculpted mesh:

    OMG! = BreastL 1.0 + Busty Breasts 1 1.0 + thicc booty & thighs 3 0.8

Morphs compose by summing their deltas, which is what VaM does and what makes a
library of region morphs worth having. Two morphs that move the same vertex both
apply, so a stack that doubles up on a region will overshoot; `strain()` reports
the largest resulting move so a recipe can be checked before it is cooked.

Axes are independent. Breast, glutes and figure each have their own tier list and
the player picks one from each, rather than CSS shipping the cross product. That
is the whole point: the 48 cooked variants in Seductress V2.1 are mostly this
cross product baked into meshes.

The stacks below name morphs from reference/CSS-v1.0.0-plan, which is the set the
body is based on. Names must match the morph file stem exactly.
"""
from __future__ import annotations

from dataclasses import dataclass, field

# Weights are at the tier's full setting. A tier is a point, and CSS interpolates
# between the tier below and above when the player is between two stops.


@dataclass(frozen=True)
class Tier:
    """One stop on an axis: a display name and the morph weights that make it."""
    id: str
    name: str
    stack: dict[str, float] = field(default_factory=dict)


@dataclass(frozen=True)
class Axis:
    """One independent body dimension the player can set."""
    id: str
    name: str
    tiers: tuple[Tier, ...]
    default: str

    def tier(self, tier_id: str) -> Tier:
        for t in self.tiers:
            if t.id == tier_id:
                return t
        raise KeyError(f'{self.id}: no tier {tier_id!r}, have {[t.id for t in self.tiers]}')


BREAST = Axis(
    id='breast', name='Bust', default='normal',
    tiers=(
        Tier('slim', 'Slim', {'MonsterShinkai_BreastS - Body': 1.0}),
        Tier('normal', 'Normal', {'MonsterShinkai_BreastM - Body': 0.5}),
        Tier('busty', 'Busty', {'MonsterShinkai_BreastM - Body': 1.0}),
        Tier('more_busty', 'More busty', {
            'MonsterShinkai_BreastL - Body': 0.8,
            'Busty Breasts 1': 0.4,
        }),
        Tier('omg', 'OMG!', {
            'MonsterShinkai_BreastL - Body': 1.0,
            'Busty Breasts 1': 1.0,
        }),
    ),
)

GLUTES = Axis(
    id='glutes', name='Glutes', default='normal',
    tiers=(
        Tier('slim', 'Slim', {}),
        Tier('normal', 'Normal', {'Booty Shape 1': 0.5}),
        Tier('full', 'Full', {'Booty Shape 4': 0.7, 'thicc booty & thighs 1': 0.4}),
        Tier('thicc', 'Thicc', {'Booty Shape 4': 1.0, 'thicc booty & thighs 2': 0.8}),
        Tier('omg', 'OMG!', {'Booty Shape 4': 1.0, 'thicc booty & thighs 3': 1.0}),
    ),
)

# The base figure. MonsterShinkai's BodyBase deliberately leaves the breast alone
# (measured: 0.07 cm average over the breast region against 1.10 over the glutes),
# which is why it stacks cleanly under the breast axis rather than fighting it.
FIGURE = Axis(
    id='figure', name='Figure', default='base',
    tiers=(
        Tier('game', 'Game default', {}),
        Tier('base', 'Sculpted', {'MonsterShinkai_BodyBase - Body': 1.0}),
        Tier('stacy', 'Stacy', {'MonsterShinkai_BodyBaseStacy - Body': 1.0}),
        Tier('thick', 'Thick', {
            'MonsterShinkai_BodyBase - Body': 1.0,
            'MonsterShinkai_BodyThickness - Body': 0.7,
        }),
    ),
)

# Small independent details, each a single morph driven by its own slider rather
# than a tier list.
DETAILS = {
    'nipples': ('Nipple size', 'MonsterShinkai_NippleSize - Body'),
    'navel': ('Navel', 'MonsterShinkai_Navel - Body'),
    'genitals': ('Genitals', 'MonsterShinkai_GenInnie - Genital'),
}

AXES = (FIGURE, BREAST, GLUTES)


def resolve(selection: dict[str, str], details: dict[str, float] | None = None) -> dict[str, float]:
    """Turn a per-axis tier choice into one flat morph stack.

    `selection` maps axis id to tier id; missing axes take their default.
    `details` maps a DETAILS key to a weight. Weights for the same morph add,
    which is deliberate: an axis and a detail may legitimately both drive one.
    """
    stack: dict[str, float] = {}
    for axis in AXES:
        tier = axis.tier(selection.get(axis.id, axis.default))
        for morph, weight in tier.stack.items():
            stack[morph] = stack.get(morph, 0.0) + weight
    for key, weight in (details or {}).items():
        if key not in DETAILS:
            raise KeyError(f'no detail {key!r}, have {sorted(DETAILS)}')
        if weight:
            morph = DETAILS[key][1]
            stack[morph] = stack.get(morph, 0.0) + weight
    return stack


def strain(stack: dict[str, float], library: dict) -> dict:
    """Largest combined move in a stack, to catch a recipe that overshoots.

    `library` maps morph name to a vam_morph.Morph. Deltas are summed per vertex
    exactly as applying them would, so this measures the real result rather than
    the largest single contributor. Returns centimetres.
    """
    combined: dict[int, tuple[float, float, float]] = {}
    missing = []
    for name, weight in stack.items():
        morph = library.get(name)
        if morph is None:
            missing.append(name)
            continue
        for i, (dx, dy, dz) in zip(morph.indices, morph.deltas):
            x, y, z = combined.get(i, (0.0, 0.0, 0.0))
            combined[i] = (x + dx * weight, y + dy * weight, z + dz * weight)
    if not combined:
        return {'largest_cm': 0.0, 'moved_vertices': 0, 'missing': missing}
    largest = max((x * x + y * y + z * z) ** .5 for x, y, z in combined.values())
    return {
        'largest_cm': largest * 100.0,
        'moved_vertices': len(combined),
        'missing': missing,
    }


def needs_graft(stack: dict[str, float], library: dict) -> list[str]:
    """Morphs in this stack that index past the G2F base and need the geograft."""
    return sorted(n for n in stack if n in library and not library[n].targets_base())


def main(argv=None):
    import argparse
    import json
    import sys
    parser = argparse.ArgumentParser(description='Resolve and check figure stacks.')
    parser.add_argument('--library', action='append', default=[],
                        help='.var package or directory of morphs (repeatable)')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args(argv)

    sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
    import vam_morph

    library = {}
    for entry in args.library:
        path = __import__('pathlib').Path(entry)
        morphs = (vam_morph.load_package(path) if path.suffix.lower() == '.var'
                  else [vam_morph.load(p) for p in sorted(path.rglob('*.vmb'))])
        for m in morphs:
            library[m.name] = m

    rows = []
    for figure in FIGURE.tiers:
        for breast in BREAST.tiers:
            for glutes in GLUTES.tiers:
                sel = {'figure': figure.id, 'breast': breast.id, 'glutes': glutes.id}
                stack = resolve(sel)
                row = {'selection': sel, 'stack': stack}
                if library:
                    row |= strain(stack, library)
                    row['needs_graft'] = needs_graft(stack, library)
                rows.append(row)

    if args.json:
        print(json.dumps(rows, indent=2))
        return 0

    print(f'{len(rows)} combinations from {len(FIGURE.tiers)}x{len(BREAST.tiers)}x{len(GLUTES.tiers)} axes')
    if not library:
        print('(pass --library to measure strain)')
    hdr = f"{'figure':<8} {'breast':<11} {'glutes':<8} {'morphs':>6}"
    if library:
        hdr += f" {'largest cm':>11} {'moved':>7}"
    print(hdr); print('-' * len(hdr))
    for r in rows:
        s = r['selection']
        line = f"{s['figure']:<8} {s['breast']:<11} {s['glutes']:<8} {len(r['stack']):>6}"
        if library:
            line += f" {r['largest_cm']:>11.2f} {r['moved_vertices']:>7}"
            if r['missing']:
                line += f"  MISSING {','.join(r['missing'])[:40]}"
        print(line)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

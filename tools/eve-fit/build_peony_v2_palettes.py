#!/usr/bin/env python3
"""Map verified V2 material recipes to the assembled character's material slots."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from css_controls import validate

NAMES = {
    'black_white_floral': 'Black and white floral',
    'cns_default': 'CNS original',
    'hydrangea_recolor': 'Hydrangea recolor',
    'peony_recolor': 'Peony recolor',
}


def build(materials, bindings):
    instances = materials['instances']
    index = {(row['palette'], row['slot']): row for row in instances}
    if len(index) != len(instances) or {key[0] for key in index} != set(NAMES):
        raise ValueError('Expected the four distinct V2 palette recipes')
    defaults = {row['package']: row for row in instances if row['palette'] == 'black_white_floral'}
    mapped = [(row['slot'], defaults[row['material']]) for row in bindings['bindings'] if row['material'] in defaults]
    if len(mapped) != 18 or len({row['slot'] for _, row in mapped}) != 18 or len({slot for slot, _ in mapped}) != 18:
        raise ValueError('Expected all 18 retained V2 parts exactly once')
    palettes = []
    for palette_id, name in NAMES.items():
        parameters = []
        for slot, original in sorted(mapped):
            instance = index[(palette_id, original['slot'])]
            if instance['material_kind'] != original['material_kind']:
                raise ValueError('A palette changes the material parent family')
            for field, kind in (('colors', 'vector'), ('scalars', 'scalar'), ('textures', 'texture')):
                if set(instance[field]) != set(original[field]):
                    raise ValueError('Palette parameter sets differ between material instances')
                for parameter, value in sorted(instance[field].items()):
                    parameters.append(dict(slot=slot, parameter=parameter, kind=kind,
                                           **{'texture' if kind == 'texture' else 'value': value}))
        palettes.append(dict(id=palette_id, name=name, values={}, parameters=parameters))
    recipe = dict(schema=1, controls=[], palettes=palettes)
    validate(recipe)
    return recipe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--materials', type=Path, required=True)
    parser.add_argument('--bindings', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    recipe = build(json.loads(args.materials.read_text()), json.loads(args.bindings.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(recipe, indent=2) + '\n')
    receipt = dict(inputs={str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in (args.materials, args.bindings, Path(__file__))},
                   output_sha256=hashlib.sha256(args.output.read_bytes()).hexdigest(),
                   parameters={p['id']: len(p['parameters']) for p in recipe['palettes']},
                   textures=sorted({r['texture'] for p in recipe['palettes'] for r in p['parameters'] if r['kind'] == 'texture'}),
                   limitation='Palette fragment only. Merge with controls and set overlapping control values before packaging. Runtime visual acceptance remains pending.')
    args.output.with_suffix('.receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt['parameters']))


if __name__ == '__main__':
    main()

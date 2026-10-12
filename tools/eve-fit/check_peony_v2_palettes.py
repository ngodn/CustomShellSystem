#!/usr/bin/env python3
"""Compare CSS palette values against decoded, cooked V2 material instances."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('recipe', 'bindings', 'decoded', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    recipe = json.loads(args.recipe.read_text())
    bindings = {r['slot']: r for r in json.loads(args.bindings.read_text())['bindings']}
    decoded = json.loads(args.decoded.read_text())
    checked = 0
    textures = set()
    for palette in recipe['palettes']:
        slots = {row['slot'] for row in palette['parameters']}
        if len(slots) != 18:
            raise ValueError('A palette does not cover all garment slots')
        for slot in slots:
            default_package = bindings[slot]['material'].split('.')[0]
            package = default_package.replace('MI_black_white_floral_', 'MI_' + palette['id'] + '_')
            instance = next(e for e in decoded[package] if e['Type'] == 'MaterialInstanceConstant')
            expected = {}
            for kind in ('Scalar', 'Vector', 'Texture'):
                for row in instance['Properties'].get(kind + 'ParameterValues', []):
                    info = row['ParameterInfo']
                    if info['Association'] != 'GlobalParameter' or info['Index'] != -1:
                        raise ValueError('Unexpected non-global source material parameter')
                    value = row['ParameterValue']
                    if kind == 'Vector':
                        value = [value[c] for c in 'RGBA']
                    elif kind == 'Texture':
                        value = value['ObjectPath'].rsplit('.', 1)[0]
                        textures.add(value)
                        if not any(e['Type'] == 'Texture2D' for e in decoded[value]):
                            raise ValueError('Palette dependency is not a cooked Texture2D')
                    expected[(info['Name'], kind.lower())] = value
            actual = {(r['parameter'], r['kind']): r for r in palette['parameters'] if r['slot'] == slot}
            if actual.keys() != expected.keys():
                raise ValueError(f'Palette parameter set differs: {palette["id"]}, slot {slot}')
            for key, expected_value in expected.items():
                row = actual[key]
                if key[1] == 'texture':
                    valid = row['texture'].rsplit('.', 1)[0] == expected_value
                else:
                    left = row['value'] if key[1] == 'vector' else [row['value']]
                    right = expected_value if key[1] == 'vector' else [expected_value]
                    valid = all(math.isclose(a, b, abs_tol=1e-6, rel_tol=1e-6) for a, b in zip(left, right, strict=True))
                if not valid:
                    raise ValueError(f'Palette differs from cooked source: {palette["id"]}, slot {slot}, {key}')
                checked += 1
    result = dict(palettes=len(recipe['palettes']), checked_parameters=checked,
                  cooked_texture_dependencies=len(textures), passed=True,
                  inputs={str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in (args.recipe, args.bindings, args.decoded, Path(__file__))},
                  limitation='Cooked material correspondence only, not runtime palette transition or visual acceptance.')
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'inputs'}))


if __name__ == '__main__':
    main()

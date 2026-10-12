"""Check V2 control targets and defaults against the cooked material readback."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('recipe', 'bindings', 'decoded', 'textures', 'root', 'output'):
        parser.add_argument('--'+key, type=Path, required=True)
    args = parser.parse_args()
    recipe, bindings, decoded, textures = [json.loads(p.read_text()) for p in
        (args.recipe, args.bindings, args.decoded, args.textures)]
    defaults = {r['slot']: r['material'].split('.')[0] for r in bindings['bindings']}
    pending = textures['pending_import']
    for path, row in pending.items():
        source = args.root/row['source']
        if hashlib.sha256(source.read_bytes()).hexdigest() != row['sha256']:
            raise ValueError('A pending texture source changed: '+path)

    def parameters(package):
        item = next(r for r in decoded[package] if r['Type'] == 'MaterialInstanceConstant')
        result = {}
        for kind in ('Scalar', 'Vector', 'Texture'):
            for row in item['Properties'].get(kind+'ParameterValues', []):
                info = row['ParameterInfo']
                if info['Association'] != 'GlobalParameter' or info['Index'] != -1:
                    raise ValueError('Unexpected non-global parameter')
                value = row['ParameterValue']
                if kind == 'Vector':
                    value = [value[c] for c in 'RGBA']
                if kind == 'Texture':
                    value = value['ObjectPath'].rsplit('.', 1)[0]
                result[info['Name']] = (kind.lower(), value)
        return result

    def near(left, right):
        return all(math.isclose(a, b, rel_tol=2e-6, abs_tol=2e-6) for a, b in zip(left, right, strict=True))

    targets = checks = choices = 0
    for control in recipe['controls']:
        for option in control.get('options', []):
            path = option['texture']
            if path.rsplit('.', 1)[0] not in decoded and path not in pending:
                raise ValueError('Unaccounted texture dependency: '+path)
            choices += 1
        for binding in control.get('bindings', []):
            slot, name = binding['slot'], binding['parameter']
            targets += 1
            for palette in [None, *recipe['palettes']]:
                package = defaults[slot]
                if palette and '/Materials/Palettes/MI_black_white_floral_' in package:
                    package = package.replace('MI_black_white_floral_', 'MI_'+palette['id']+'_')
                kind, expected = parameters(package)[name]
                value = palette['values'].get(control['id'], control['default']) if palette else control['default']
                if control['kind'] == 'choice':
                    valid = kind == 'texture' and control['options'][int(value[0])]['texture'].rsplit('.', 1)[0] == expected
                elif control['kind'] == 'color':
                    actual = [v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in value[:3]] + value[3:]
                    # SourceGlow feeds opaque BaseColor/Emissive RGB. Its unused
                    # alpha differs across source palettes and is protected in CSS.
                    valid = kind == 'vector' and near(actual[:3] if name == 'SourceGlowColor' else actual,
                                                      expected[:3] if name == 'SourceGlowColor' else expected)
                else:
                    valid = kind == 'scalar' and near([value[0]], [expected])
                if not valid:
                    raise ValueError(f'Control changes the authored look: {control["id"]}, {package}, {name}')
                checks += 1
    result = dict(passed=True, targets=targets, original_and_palette_checks=checks,
                  texture_choices=choices, pending_texture_imports=len(pending),
                  inputs={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in
                          (args.recipe, args.bindings, args.decoded, args.textures, Path(__file__))},
                  limitation='Static cooked bindings and defaults only; seven pending source textures require cooking before installation.')
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'inputs'}))


if __name__ == '__main__':
    main()

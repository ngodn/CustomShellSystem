"""Compare a passive Peony material capture with its packaged palette and controls."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def verify(capture, customize):
    if not capture.get('requests_finished') or not capture.get('selection_stable'):
        raise ValueError('Capture incomplete or selection changed')
    selection = capture['status_before']['customize']
    palette = next((p for p in customize['palettes'] if p['id'] == selection['palette']), None)
    if not palette:
        raise ValueError('This check requires a named palette')
    instances = {row['object']['$object']: row for row in capture['materials']}

    def parameters(handle, trail=()):
        key = handle['$object']
        if key in trail or len(trail) > 8:
            raise ValueError('Material parent cycle or excessive depth')
        if key not in instances:
            return {}
        row = instances[key]
        resolved = parameters(row['Parent'], (*trail, key)) if row.get('Parent') else {}
        for kind in ('Scalar', 'Vector', 'Texture'):
            for parameter in row[kind + 'ParameterValues']:
                info = parameter['ParameterInfo']
                name = (info['Name'], info['Association'], info['Index'])
                value = parameter['ParameterValue']
                if kind == 'Vector':
                    value = [value[c] for c in 'RGBA']
                elif kind == 'Texture':
                    value = value['name'].split(' ', 1)[1] if value else None
                resolved[name] = (kind.lower(), value)
        return resolved

    expected = {}
    for item in palette['parameters']:
        expected[(item['slot'], item['parameter'])] = (
            item['kind'], item['texture'] if item['kind'] == 'texture' else item['value'])
    values = dict(palette['values']) | selection['values']
    control_bindings = 0
    for control in customize['controls']:
        if not control.get('bindings'):
            continue
        value = values.get(control['id'], control['default'])
        for binding in control.get('bindings', []):
            slot = binding['slot']
            if slot not in (*range(20, 38), *range(41, 46)):
                continue
            if binding.get('association', 'global') != 'global' or binding.get('layer', -1) != -1:
                raise ValueError('Checker only supports global parameters')
            kind = control['kind']
            if kind == 'choice':
                index = math.floor(value[0] + .5)
                result = ('texture', control['options'][index]['texture'])
            elif kind == 'color':
                result = ('vector', [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
                                     for v in value[:3]] + value[3:])
            elif kind in ('scalar', 'glow', 'opacity', 'intensity'):
                result = ('scalar', value[0])
            else:
                raise ValueError('Unsupported bound control kind: ' + kind)
            expected[(slot, binding['parameter'])] = result
            control_bindings += 1

    resolved = {slot: parameters(capture['slots_before'][slot]) for slot, _ in expected}
    failures = []
    for (slot, name), (kind, value) in expected.items():
        actual = resolved[slot].get((name, 2, -1))
        match = actual is not None and actual[0] == kind
        if match:
            if kind == 'texture':
                match = actual[1] == value
            else:
                left, right = (actual[1], value) if kind == 'vector' else ([actual[1]], [value])
                match = len(left) == len(right) and all(math.isclose(a, b, rel_tol=3e-6, abs_tol=3e-6)
                                                       for a, b in zip(left, right))
        if not match:
            failures.append(dict(slot=slot, parameter=name, expected=[kind, value], actual=actual))
    return dict(passed=not failures, palette=palette['id'], unique_parameters=len(expected),
                control_bindings=control_bindings, slots=len(resolved), failures=failures,
                limitation='One captured selection; no visual, visibility, profile reload or GPU evaluation proof.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    parser.add_argument('--manifest', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise RuntimeError('Refusing to overwrite a report')
    manifest = json.loads(args.manifest.read_text())
    outfit = next(o for o in manifest['catalog']['outfits'] if o['id'] == 'eins0fx.seduxtress')
    variant = next(v for v in outfit['variants'] if v['id'] == 'peony')
    result = verify(json.loads(args.capture.read_text()), variant['customize'])
    result['sha256'] = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in (args.capture, args.manifest, Path(__file__))}
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, indent=2))
    if not result['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

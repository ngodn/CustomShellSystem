#!/usr/bin/env python3
"""Read-only check of the material branch isolated in the Harros dodge capture.

Requires an applied CSS outfit and developer core. This verifies the runtime
parameter; the frozen-pose screenshots remain the
visual regression evidence. It does not claim to measure cloth or motion blur.
"""
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from cssx_cheat_check import request as host
from cssx_dev import MOD

PARAMETER = 'DarkBro WPO Stretch Scale'

def call(target, name, **args):
    return host(dict(op='call', target=target, function=name, args=args), True)['ReturnValue']

def main():
    status = json.loads((MOD / 'runtime/status.json').read_text())
    assert status['applied'], 'Select a CSS outfit first'
    pawn = host({'op': 'player'}, True)['pawn']
    mesh = host(dict(op='get', target=pawn, property='Mesh'), True)
    rows = []
    for slot in range(call(mesh, 'GetNumMaterials')):
        material = call(mesh, 'GetMaterial', ElementIndex=slot)
        if not material:
            continue
        if material['class'] == 'Class /Script/Engine.Material':
            rows.append(dict(slot=slot, material=material['name'], skipped='standalone material'))
            continue
        value = call(material, 'K2_GetScalarParameterValue', ParameterName=PARAMETER)
        rows.append(dict(slot=slot, material=material['name'], scale=value))
    print(json.dumps(dict(outfit=status['applied'], materials=rows), indent=2), flush=True)
    assert rows, 'No outfit materials were checked'
    assert any('scale' in row for row in rows), 'No material instance was checked'
    assert all(row.get('scale', 0) == 0 for row in rows), 'Outfit still responds to incompatible DarkBro displacement'
    print('All applied material instances have neutral DarkBro stretching.')

if __name__ == '__main__':
    main()

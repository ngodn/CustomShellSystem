"""Verify option textures retain source dimensions and usable cooked mip data."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
for key in ('root', 'plan', 'decoded', 'output'):
    parser.add_argument('--'+key, type=Path, required=True)
args = parser.parse_args()
plan = json.loads(args.plan.read_text())['pending_import']
decoded = json.loads(args.decoded.read_text())
assert {path.rsplit('.', 1)[0] for path in plan} == set(decoded)
checks = []
for path, row in plan.items():
    source = args.root/row['source']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == row['sha256']
    with Image.open(source) as image:
        size = image.size
    export, = decoded[path.rsplit('.', 1)[0]]
    assert export['Type'] == 'Texture2D'
    assert (export['SizeX'], export['SizeY']) == size
    assert export['Properties'].get('SRGB', True) is True
    assert export['PixelFormat'] in ('PF_DXT1', 'PF_DXT5', 'PF_BC7')
    assert export['FirstMipToSerialize'] == 0 and export['Mips']
    width, height = size
    for mip in export['Mips']:
        assert (mip['SizeX'], mip['SizeY'], mip['SizeZ']) == (width, height, 1)
        assert mip['BulkData']['ElementCount'] > 0
        width, height = max(width//2, 1), max(height//2, 1)
    assert export['Mips'][-1]['SizeX'] == export['Mips'][-1]['SizeY'] == 1
    checks.append(dict(path=path, size=size, mips=len(export['Mips']), format=export['PixelFormat']))
result = dict(passed=True, textures=checks, inputs={str(p): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in (args.plan, args.decoded, Path(__file__))},
              limitation='Cooked dimensions, mip metadata and source hashes only. Runtime appearance remains unverified.')
args.output.write_text(json.dumps(result, indent=2)+'\n')
print('Verified', len(checks), 'cooked option textures')

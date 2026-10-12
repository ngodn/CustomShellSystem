"""Validate the gray control and measure the fixed-exposure Peony captures."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import OpenEXR
from PIL import Image


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stage', type=Path)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
report = json.loads((args.stage / 'result.json').read_text())
assert report['compilation']['passed'] and report['inputs_unchanged']
assert report['ldr_format'] == 'RTF_RGBA8_SRGB'
assert report['hdr_format'] == 'RTF_RGBA16F'
assert report['gray_control_emission'] == .18
assert len(report['images']) == 24
images = {}
for row in report['images']:
    path = args.stage / 'captures' / row['image']
    assert path.parent == args.stage / 'captures'
    assert hashlib.sha256(path.read_bytes()).hexdigest() == row['sha256']
    key = (row['view'], row['key_lux'], row['fill_lux'], row['glow'], path.suffix)
    assert key not in images
    if path.suffix == '.exr':
        with OpenEXR.File(str(path)) as image:
            pixels = image.channels()['RGBA'].pixels[:, :, :3].astype(np.float64)
    else:
        pixels = np.asarray(Image.open(path).convert('RGB'), dtype=np.float64)
    assert pixels.shape == (640, 640, 3) and np.isfinite(pixels).all()
    images[key] = pixels

rows = []
for view in ('front', 'back'):
    x = slice(535, 556) if view == 'front' else slice(84, 105)
    control = None
    for key, fill in ((3., .15), (.15, 3.), (1., 1.)):
        off = images[view, key, fill, 0., '.exr']
        on = images[view, key, fill, 17.25, '.exr']
        for glow in (0., 17.25):
            hdr = images[view, key, fill, glow, '.exr'][62:87, x]
            assert np.max(np.abs(hdr - .18)) < .001, 'Gray control is missing or incorrect'
            ldr = images[view, key, fill, glow, '.png'][62:87, x]
            if control is None:
                control = ldr
            assert np.array_equal(control, ldr), 'Display exposure changed across the matrix'
        # Fixed corset crop, including any narrow trim crossing its edges.
        roi = (slice(195, 230), slice(295, 305))
        rows.append(dict(view=view, key_lux=key, fill_lux=fill,
                         corset_roi_hdr_mean=float(off[roi].mean()),
                         corset_roi_glow_min_difference=float((on[roi] - off[roi]).min()),
                         corset_roi_glow_max_difference=float(np.abs(on[roi] - off[roi]).max()),
                         gray_hdr_mean=float(off[62:87, x].mean()),
                         gray_png_mean=float(control.mean())))
result = dict(control_checks_passed=True, images=len(images), rows=rows,
              scope=report['scope'], limitation='Fixed-exposure direct-light fixture only. '
              'No conclusion about the game exposure, bloom, environment or skin.')
with args.output.open('x') as stream:
    json.dump(result, stream, indent=2)
    stream.write('\n')
print(json.dumps(result, indent=2))

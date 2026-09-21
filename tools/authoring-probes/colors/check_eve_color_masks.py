"""Eve's reviewed swatch brightness and UV boundary regressions. Python 3.14."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image


def inspect(folder):
    recipe = json.loads((folder / 'customize.json').read_text())['customize']
    def pixels(name):
        return np.asarray(Image.open(folder / name).convert('RGBA'))
    body = pixels('dye-skin-body.png')
    failures = []
    # Multiplicative swatches need a near-white detail layer. The rejected gray
    # layer produced RGB 148,128,107 for Ivory 221,192,162 in the real renderer.
    median = float(np.median(body[:, :, 0][body[:, :, 3] > 250]))
    if median < 230:
        failures.append(f'Body detail layer dims the swatch: median {median}, minimum 230')
    edge = pixels('dye-areola.png')[:, :, 3]
    transition = int(np.count_nonzero((edge > 0) & (edge < 255)))
    if transition < 100:
        failures.append('Areola mask has no useful soft transition')
    surface = next(s for s in recipe['surfaces'] if s['id'] == 'surface_5')
    skin_file = surface['layers'].get('skin')
    if not skin_file:
        failures.append('Material 5 surrounding skin is omitted from skin coloring')
    else:
        a = pixels(skin_file)[:, :, 3]
        for x, y in [(512, 870), (475, 890), (547, 890)]:
            if a[y, x] != 255:
                failures.append(f'Surrounding skin is not fully covered at {x},{y}')
        if a[941, 512] != 0:
            failures.append('Skin dye overlaps the protected central region')
    return dict(passed=not failures, body_median=median,
                soft_edge_texels=transition, failures=failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('metadata', type=Path)
    args = parser.parse_args()
    result = inspect(args.metadata)
    print(json.dumps(result, indent=2))
    raise SystemExit(not result['passed'])


if __name__ == '__main__':
    main()

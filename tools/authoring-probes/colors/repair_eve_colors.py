"""Repair Eve's authored dye brightness and boundaries. Python 3.14.

UV coordinates below are hand-authored against Eve's accepted 1024px atlases.
They are not a general anatomical classifier or a skeleton modification.
"""
import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image

from prepare_eve_colors import CSS, MOD
from check_eve_color_masks import inspect
from css import atomic, sha, copy_verified
from css_controls import embed, validate, lint_convention
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT
from css_package import verify


def linear(rgb):
    return np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)


def srgb(rgb):
    return np.where(rgb <= .0031308, rgb * 12.92, 1.055 * rgb ** (1 / 2.4) - .055)


def ellipse(cx, cy, rx, ry, inner):
    y, x = np.mgrid[:1024, :1024]
    radius = np.sqrt(((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2)
    t = np.clip((radius - inner) / (1 - inner), 0, 1)
    return 1 - t * t * (3 - 2 * t)


def load(path):
    return np.asarray(Image.open(path).convert('RGBA'))


def detail(source, reference):
    # The chip is the desired shade, not an extra darkening multiplier. A shared
    # skin reference keeps connected body atlases on the same brightness scale.
    values = linear(source[:, :, :3] / 255)
    return np.rint(srgb(np.clip(values / linear(reference / 255), 0, 1)) * 255).astype(np.uint8)


def write_layer(folder, name, source, coverage, reference):
    result = source.copy()
    result[:, :, :3] = detail(source, reference)
    result[:, :, 3] = np.rint(source[:, :, 3] * coverage).astype(np.uint8)
    Image.fromarray(result).save(folder / name)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    assert out.is_relative_to(CSS.parent) and not out.exists()
    old = MOD / 'work/colors3'
    original = verify(old / 'trio')
    out.mkdir()
    metadata = out / 'metadata'
    shutil.copytree(old / 'metadata', metadata)
    data = json.loads((metadata / 'customize.json').read_text())
    recipe = data['customize']
    previous = copy.deepcopy(recipe)
    body = load(MOD / 'work/ground1/metadata/dye-02.png')
    # Shared reference is the accepted body's 95th-percentile gray value (180).
    reference = float(np.percentile(body[:, :, 0][body[:, :, 3] > 127], 95))
    for i in (0, 1, 3, 4):
        source = load(metadata / f'dye-{i:02}.png')
        write_layer(metadata, f'dye-{i:02}.png', source, np.ones((1024, 1024)), reference)
    areola = np.maximum(ellipse(438, 491, 22, 23, .60), ellipse(588, 491, 22, 23, .60))
    nipple = np.maximum(ellipse(438, 491, 6, 6, .50), ellipse(588, 491, 6, 6, .50))
    assert np.all(nipple <= areola)
    write_layer(metadata, 'dye-skin-body.png', body, 1 - areola, reference)
    # Region-specific references prevent multiplying the chosen shade by the
    # original pigment a second time. Shadows and surface variation remain.
    for name, coverage in [('dye-nipple.png', nipple), ('dye-areola.png', areola - nipple)]:
        region_reference = float(np.percentile(body[:, :, 0][coverage > .95], 90))
        write_layer(metadata, name, body, coverage, region_reference)
    source = load(MOD / 'work/ground1/metadata/dye-05.png')
    central = np.zeros((1024, 1024))
    central[:, 256:768] = 1
    # The former whole-island selection included the upper and lateral skin.
    # The authored ellipse follows only the pigmented center visible in 05_BC.
    labia = ellipse(512, 940, 13, 37, .55) * central
    write_layer(metadata, 'dye-skin-groin.png', source, central - labia, reference)
    region_reference = float(np.percentile(source[:, :, 0][labia > .95], 90))
    write_layer(metadata, 'dye-labia.png', source, labia, region_reference)
    interior = (1 - central) * (source[:, :, 3] > 0)
    region_reference = float(np.percentile(source[:, :, 0][interior > .95], 90))
    write_layer(metadata, 'dye-genitals.png', source, interior, region_reference)
    surface = next(s for s in recipe['surfaces'] if s['id'] == 'surface_5')
    surface['layers']['skin'] = 'dye-skin-groin.png'
    validate(recipe)
    assert not lint_convention(recipe)
    unchanged = copy.deepcopy(recipe)
    next(s for s in unchanged['surfaces'] if s['id'] == 'surface_5')['layers'].pop('skin')
    assert unchanged == previous, 'Unexpected control or palette change'
    atomic(metadata / 'customize.json', data)
    catalog = json.loads((metadata / 'catalog.json').read_text())
    catalog['outfits'][0]['variants'][0]['customize'] = copy.deepcopy(recipe)
    atomic(metadata / 'catalog.json', catalog)
    checks = inspect(metadata)
    assert checks['passed'], checks
    # Alpha coverage remains complete up to byte rounding at soft transitions.
    for original_file, names in [('dye-02.png', ['dye-skin-body.png', 'dye-nipple.png', 'dye-areola.png']),
                                 ('dye-05.png', ['dye-skin-groin.png', 'dye-labia.png', 'dye-genitals.png'])]:
        expected = load(MOD / 'work/ground1/metadata' / original_file)[:, :, 3].astype(int)
        actual = sum(load(metadata / name)[:, :, 3].astype(int) for name in names)
        assert np.max(np.abs(expected - actual)) <= 1, 'Coverage was lost or duplicated'
    stage = out / 'stage'
    pak = next((old / 'trio').glob('*.pak'))
    subprocess.run([str(DEFAULT_REPAK), 'unpack', str(pak), '--output', str(stage)], check=True)
    folder = stage / PACKAGE_ROOT / original['id']
    manifest = copy.deepcopy(original)
    manifest['resources'] = {}
    for path in folder.glob('dye-*.png'):
        path.unlink()
    variant = manifest['catalog']['outfits'][0]['variants'][0]
    embed(metadata / 'customize.json', manifest, folder, variant=variant)
    atomic(folder / 'manifest.json', manifest)
    trio = out / 'trio'
    trio.mkdir()
    for suffix in ('.ucas', '.utoc'):
        copy_verified(pak.with_suffix(suffix), trio / pak.with_suffix(suffix).name)
    subprocess.run([str(DEFAULT_REPAK), 'pack', str(stage), str(trio / pak.name), '--version', 'V8B'], check=True)
    assert verify(trio) == manifest
    reverse = copy.deepcopy(manifest)
    reverse['resources'] = original['resources']
    reverse['catalog']['outfits'][0]['variants'][0]['customize'] = original['catalog']['outfits'][0]['variants'][0]['customize']
    assert reverse == original
    atomic(out / 'verification.json', dict(checks=checks, skin_reference=reference,
        hashes={p.name: sha(p) for p in trio.iterdir()},
        scope='Dye textures and one additional skin layer only. Controls, palettes, hair resources and cooked containers retained.',
        pending='Live appearance review; soft-region independence and reset behavior.'))
    print('Prepared', trio)


if __name__ == '__main__':
    main()

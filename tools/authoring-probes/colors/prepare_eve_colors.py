"""Prepare Eve's color metadata without recooking her accepted mesh. Python 3.14."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

CSS = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(CSS / 'tools'))
from css import atomic, sha, copy_verified
from css_controls import embed, validate, lint_convention
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT
from css_package import verify

MOD = CSS.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'


def rgba(hex_color):
    return [round(int(hex_color[i:i + 2], 16) / 255, 7) for i in (0, 2, 4)] + [1]


def choices(default, entries):
    result = [dict(name='Default', color=rgba(default), reset=True)]
    result.extend(dict(name=name, color=rgba(color)) for name, color in entries)
    assert len(result) == 12
    return result


FABRIC = [('Obsidian', '17191E'), ('Pearl', 'DDD9CA'), ('Garnet', '751E38'),
          ('Midnight', '203758'), ('Jade', '226A54'), ('Amethyst', '654178'),
          ('Rose', 'B67383'), ('Glacier', '76A6B8'), ('Champagne', 'BCA675'),
          ('Copper', '9B583D'), ('Silver', 'A5ADB8')]
SKIN = [('Porcelain', 'F1DACE'), ('Alabaster', 'E8CBB9'), ('Ivory', 'DDC0A2'),
        ('Peach', 'DBAE97'), ('Sand', 'C6A587'), ('Olive', 'B89C77'),
        ('Honey', 'B8885B'), ('Caramel', '9D714F'), ('Bronze', '835B42'),
        ('Cocoa', '674334'), ('Ebony', '49312C')]
ANATOMY = [('Blush', 'D2958F'), ('Rose', 'BC7C77'), ('Peach', 'C58F7B'),
           ('Dusty rose', 'A97170'), ('Terracotta', 'AA7661'), ('Mauve', '976A73'),
           ('Caramel', '997054'), ('Chestnut', '865B49'), ('Rosewood', '774B4C'),
           ('Cocoa', '644539'), ('Umber', '50382F')]
HAIR = [('Black', '111319'), ('Espresso', '28201E'), ('Chestnut', '51362A'),
        ('Auburn', '74392B'), ('Copper', 'A15C37'), ('Honey', 'B59863'),
        ('Platinum', 'DCD5C2'), ('Silver', 'A5ADB8'), ('Blue black', '1C2B40'),
        ('Violet', '503957'), ('Burgundy', '542635')]
METAL = [('Antique gold', 'AB8A48'), ('Champagne', 'C9B27A'), ('Brass', 'A18C52'),
         ('Bronze', '89663E'), ('Copper', 'AD7051'), ('Rose gold', 'B48B79'),
         ('Silver', 'BBC3CC'), ('Steel', '89929C'), ('Pewter', '6D777D'),
         ('Gunmetal', '424B55'), ('Obsidian', '20252C')]


def shape_mask(source, name):
    image = Image.new('L', (1024, 1024))
    draw = ImageDraw.Draw(image)
    deltas = source['shapes'][name]
    for triangle in source['triangles']:
        if triangle['material'] != 2 or not all(deltas[i] > 1e-6 for i in triangle['vertices']):
            continue
        assert all(2 <= u < 3 and 0 <= v <= 1 for u, v in triangle['uv'])
        draw.polygon([((u - 2) * 1024, (1 - v) * 1024) for u, v in triangle['uv']], fill=255)
    # One texel covers raster boundary rounding. No painted RGB or geometry changes.
    return np.asarray(image.filter(ImageFilter.MaxFilter(3))) > 0


def partition(original, masks, folder):
    pixels = np.asarray(Image.open(original).convert('RGBA'))
    used = np.zeros(pixels.shape[:2], dtype=bool)
    counts = {}
    for name, region in masks.items():
        assert region.shape == used.shape and not np.any(region & used)
        used |= region
        result = pixels.copy()
        result[:, :, 3] = np.where(region, pixels[:, :, 3], 0)
        counts[name] = int(np.count_nonzero(result[:, :, 3]))
        assert counts[name] > 0
        Image.fromarray(result).save(folder / name)
    assert np.all(used[pixels[:, :, 3] > 0]), 'Partition dropped original coverage'
    restored = sum(np.asarray(Image.open(folder / name))[:, :, 3].astype(np.uint16) for name in masks)
    assert np.array_equal(restored, pixels[:, :, 3]), 'Partition changed coverage'
    return counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    assert out.is_relative_to(CSS.parent) and not out.exists()
    old = MOD / 'work/ground1'
    original = verify(old / 'trio')
    assert original['catalog']['outfits'][0]['variants'][0]['ground_offset_cm'] == -3
    source = json.loads(args.source.read_text())
    out.mkdir()
    metadata = out / 'metadata'
    shutil.copytree(old / 'metadata', metadata)
    data = json.loads((metadata / 'customize.json').read_text())
    recipe = data['customize']
    protected = {c['id']: copy.deepcopy(c) for c in recipe['controls'] if c['kind'] != 'color'}
    nipple = shape_mask(source, 'PBMNipplesDepth')
    areola = shape_mask(source, 'PBMAreolaeDepth') | nipple
    counts = partition(old / 'metadata/dye-02.png', {
        'dye-skin-body.png': ~areola,
        'dye-nipple.png': nipple,
        'dye-areola.png': areola & ~nipple,
    }, metadata)
    # Material 5 has three separate UV islands: an external central patch and
    # paired lining islands at the atlas edges. Keep the latter independently dyed.
    central = np.zeros((1024, 1024), dtype=bool)
    central[:, 256:768] = True
    for t in source['triangles']:
        if t['material'] != 5:
            continue
        u = [p[0] - 2 for p in t['uv']]
        assert all(.4 < x < .6 for x in u) or all(x < .1 for x in u) or all(x > .9 for x in u)
    counts.update(partition(old / 'metadata/dye-05.png', {
        'dye-labia.png': central, 'dye-genitals.png': ~central,
    }, metadata))
    by_id = {c['id']: c for c in recipe['controls']}
    by_id['skin']['swatches'] = choices('DEC0AE', SKIN)
    for name in ('suit_color', 'stocking_color', 'shoe_color'):
        by_id[name]['swatches'] = choices('17191E', FABRIC)
    by_id['hair_color']['swatches'] = choices('111319', HAIR)
    by_id['hair_accessory_color']['swatches'] = choices('35588C', METAL)
    additions = []
    for id_, name, role in [('nipple_color', 'Nipples', 'nipple'), ('areola_color', 'Areolae', 'areola'),
                            ('labia_color', 'Labia', 'labia'), ('genital_color', 'Genital lining', 'vestibule')]:
        additions.append(dict(id=id_, name=name, kind='color', group='body', role=role,
                              hue_locked=True, default=[1, 1, 1, 1],
                              swatches=choices('BC8B80', ANATOMY)))
    index = recipe['controls'].index(by_id['skin']) + 1
    recipe['controls'][index:index] = additions
    for surface in recipe['surfaces']:
        if surface['id'] == 'surface_2':
            assert surface['layers'] == {'skin': 'dye-02.png'}
            surface['layers'] = {'skin': 'dye-skin-body.png', 'nipple_color': 'dye-nipple.png', 'areola_color': 'dye-areola.png'}
        elif surface['id'] == 'surface_5':
            assert surface['layers'] == {'skin': 'dye-05.png'}
            surface['layers'] = {'labia_color': 'dye-labia.png', 'genital_color': 'dye-genitals.png'}
    palette_rows = [
        ('ivory', 'Moonstone', 'DDD9CA', '676C76', 'D6CEB9', 'C9B27A'),
        ('crimson', 'Garnet', '751E38', '332832', '431E2C', 'B48B79'),
        ('midnight', 'Abyss', '203758', '232D3C', '172237', 'BBC3CC'),
        ('jade', 'Jade', '226A54', '243F38', '173D31', 'AB8A48'),
        ('amethyst', 'Amethyst', '654178', '392D43', '422953', 'A5ADB8'),
    ]
    outfit_colors = ('suit_color', 'stocking_color', 'shoe_color', 'hair_accessory_color')
    recipe['palettes'] = [dict(id=id_, name=name, values={id_: rgba(color) for id_, color in zip(outfit_colors, colors)})
                          for id_, name, *colors in palette_rows]
    assert {c['id']: c for c in recipe['controls'] if c['kind'] != 'color'} == protected
    assert len(recipe['palettes']) == 5 and all(set(p['values']) == set(outfit_colors) for p in recipe['palettes'])
    assert all(len(c['swatches']) == 12 for c in recipe['controls'] if c['kind'] == 'color')
    validate(recipe)
    assert not lint_convention(recipe), lint_convention(recipe)
    atomic(metadata / 'customize.json', data)
    catalog = json.loads((metadata / 'catalog.json').read_text())
    assert len(catalog['outfits'][0]['variants']) == 1
    catalog['outfits'][0]['variants'][0]['customize'] = copy.deepcopy(recipe)
    atomic(metadata / 'catalog.json', catalog)
    stage = out / 'stage'
    pak = next((old / 'trio').glob('*.pak'))
    subprocess.run([str(DEFAULT_REPAK), 'unpack', str(pak), '--output', str(stage)], check=True)
    folder = stage / PACKAGE_ROOT / original['id']
    manifest = copy.deepcopy(original)
    manifest['resources'] = {}
    for path in folder.glob('dye-*.png'):
        path.unlink()
    assert len(manifest['catalog']['outfits'][0]['variants']) == 1
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
    assert reverse == original, 'Unrelated package metadata changed'
    atomic(out / 'verification.json', dict(passed=True, source=str(args.source), source_sha256=sha(args.source),
        counts=counts, palettes=[p['name'] for p in recipe['palettes']],
        controls=len(recipe['controls']), swatches_per_color=12,
        hashes={p.name: sha(p) for p in trio.iterdir()},
        scope='Metadata and partitioned dye resources only; cooked containers and non-color controls preserved exactly.',
        pending='Visual region boundaries, palette appearance and live menu verification.'))
    print('Prepared', trio)


if __name__ == '__main__':
    main()

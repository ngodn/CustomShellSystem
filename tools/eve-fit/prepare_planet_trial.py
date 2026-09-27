"""Build an isolated Eve fitting package from verified containers. Python 3.14."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css_convert import Converter, DEFAULT_REPAK, PACKAGE_ROOT, digest
from css_package import verify


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--revision', type=int, choices=(13,14,16), default=13)
    parser.add_argument('--kind', choices=('prototype','skin'), default='prototype')
    args=parser.parse_args();revision=args.revision;skin=args.kind=='skin'
    assert (skin and revision==16) or (not skin and revision in (13,14))
    prefix='s' if skin else 'p'
    stem=f'CSS_EveSkinFit{revision}_P' if skin else f'CSS_EveFit{revision}_P'
    identity=f'eins0fx.eveskinfit{revision}' if skin else f'eins0fx.evefit{revision}'
    work = ROOT / 'work/eve26'
    packed = work / ('s16pack' if skin else ('p13pack2' if revision==13 else 'p14pack'))
    proof = json.loads((packed / 'verification.json').read_text())
    for name, expected in proof['containers'].items():
        assert digest(packed / name) == expected
    for name, expected in proof['installed_eve_dependencies'].items():
        assert digest(packed / 'containers' / name) == expected
    source = work / 'p13meta' / PACKAGE_ROOT / 'eins0fx.seduxtress'
    original = json.loads((source / 'manifest.json').read_text())
    manifest = copy.deepcopy(original)
    outfit = manifest['catalog']['outfits'][0]
    variant = next(v for v in outfit['variants'] if v['id'] == ('skin_suit' if skin else 'planet_diving'))
    controls = json.loads((work / ('skin-lining6/controls.json' if skin else 'planet-sections/controls.json')).read_text())
    controls.append(dict(id='hair', name='Hair', kind='toggle', role='piece',
                         default=[1, 0, 0, 1], sections=[17,18] if skin else [18,19]))
    controls.extend(c for c in variant['customize']['controls'] if c['kind'] == 'shape')
    assert len(controls) == (8 if skin else 11)
    available = {e['name'] for e in proof['assets'][0]['exports']['exports'] if e['class'] == 'MorphTarget'}
    assert all(c['morph'] in available for c in controls if c['kind'] == 'shape')
    mesh_name=f'SK_SFit{revision}' if skin else f'SK_PFit{revision}'
    variant.update(id='skin_suit' if skin else 'prototype', name='Skin Suit' if skin else 'Prototype Planet Diving Suit',
                   mesh=f'/Game/CSS/EveTest/{mesh_name}.{mesh_name}',
                   customize=dict(schema=1, controls=controls, surfaces=[], palettes=[]))
    description = f'Private F{revision} fitting trial. Requires the installed Eve package. Colors and final release validation remain unfinished.'
    for entry in (manifest, outfit):
        entry.update(id=identity, name='Eve Skin Suit Fitting Trial' if skin else 'Eve Prototype Fitting Trial',
                     version=f'0.0.{revision}', description=description)
    outfit['variants'] = [variant]
    outfit.pop('templates', None)
    manifest['resources'] = {}
    output = work / f'{prefix}{revision}trial'
    output.mkdir(exist_ok=False)
    metadata = output / 'metadata' / PACKAGE_ROOT / manifest['id']
    metadata.mkdir(parents=True)
    shutil.copy2(source / 'thumbnail.png', metadata / 'thumbnail.png')
    trio = output / stem
    trio.mkdir()
    for suffix in ('.utoc', '.ucas'):
        path = trio / (stem + suffix)
        shutil.copy2(packed / path.name, path)
        manifest['containers'][suffix] = dict(file=path.name, bytes=path.stat().st_size, sha256=digest(path))
    (metadata / 'manifest.json').write_text(json.dumps(manifest, separators=(',', ':')))
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    converter.run(DEFAULT_REPAK, 'pack', output / 'metadata', trio / (stem + '.pak'), '--version', 'V8B')
    assert verify(trio) == manifest
    (output / 'verification.json').write_text(json.dumps(dict(
        manifest_verified=True, independent_controls=[c['id'] for c in controls],
        dependencies=proof['installed_eve_dependencies'],
        files={p.name:digest(p) for p in trio.iterdir()},
        scope='Private fitting trial only. Not a standalone release. Game visibility, cloth motion and profile persistence pending.'
    ), indent=2) + '\n')


if __name__ == '__main__':
    main()

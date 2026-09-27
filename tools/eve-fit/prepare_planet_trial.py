"""Build an isolated F13 fitting package from verified containers. Python 3.14."""
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
    work = ROOT / 'work/eve26'
    packed = work / 'p13pack2'
    proof = json.loads((packed / 'verification.json').read_text())
    for name, expected in proof['containers'].items():
        assert digest(packed / name) == expected
    for name, expected in proof['installed_eve_dependencies'].items():
        assert digest(packed / 'containers' / name) == expected
    source = work / 'p13meta' / PACKAGE_ROOT / 'eins0fx.seduxtress'
    original = json.loads((source / 'manifest.json').read_text())
    manifest = copy.deepcopy(original)
    outfit = manifest['catalog']['outfits'][0]
    variant = next(v for v in outfit['variants'] if v['id'] == 'planet_diving')
    controls = json.loads((work / 'planet-sections/controls.json').read_text())
    controls.append(dict(id='hair', name='Hair', kind='toggle', role='piece',
                         default=[1, 0, 0, 1], sections=[18, 19]))
    controls.extend(c for c in variant['customize']['controls'] if c['kind'] == 'shape')
    assert len(controls) == 11
    available = {e['name'] for e in proof['assets'][0]['exports']['exports'] if e['class'] == 'MorphTarget'}
    assert all(c['morph'] in available for c in controls if c['kind'] == 'shape')
    variant.update(id='prototype', name='Prototype Planet Diving Suit',
                   mesh='/Game/CSS/EveTest/SK_PFit13.SK_PFit13',
                   customize=dict(schema=1, controls=controls, surfaces=[], palettes=[]))
    description = 'Private F13 fitting trial. Requires the installed Eve package. Colors and final release validation remain unfinished.'
    for entry in (manifest, outfit):
        entry.update(id='eins0fx.evefit13', name='Eve Prototype Fitting Trial',
                     version='0.0.13', description=description)
    outfit['variants'] = [variant]
    outfit.pop('templates', None)
    manifest['resources'] = {}
    output = work / 'p13trial'
    output.mkdir(exist_ok=False)
    metadata = output / 'metadata' / PACKAGE_ROOT / manifest['id']
    metadata.mkdir(parents=True)
    shutil.copy2(source / 'thumbnail.png', metadata / 'thumbnail.png')
    stem = 'CSS_EveFit13_P'
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

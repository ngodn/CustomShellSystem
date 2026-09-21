"""Prepare Eve's public package name while preserving saved-look IDs. Python 3.14."""
import argparse
import copy
from pathlib import Path
import subprocess
import sys

CSS = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(CSS / 'tools'))
from css import atomic, copy_verified, sha
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT
from css_package import verify
from css_paths import component, output_path, package_path

STEM = 'CSS_EveStellarBlade_eins0fx_P'
NAME = 'Eve (Stellar Blade)'
DESCRIPTION = ('Eve from Stellar Blade with the Black Pearl outfit, modular outfit parts, '
               'color palettes, body shapes and secondary motion.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    out = args.output.resolve()
    assert out.is_relative_to(CSS / 'work') and not out.exists()
    component(STEM)
    output_path(out)
    original = verify(source)
    assert original['id'] == 'eins0fx.seduxtress'
    assert original['name'] == 'SeduXtress: Black Pearl'
    assert original['version'] == '1.0.0-candidate'
    old_variant = original['catalog']['outfits'][0]['variants']
    assert len(old_variant) == 1 and old_variant[0]['id'] == 'black_pearl'
    assert old_variant[0]['ground_offset_cm'] == -3
    out.mkdir()
    stage = out / 'stage'
    source_pak = next(source.glob('*.pak'))
    subprocess.run([str(DEFAULT_REPAK), 'unpack', str(source_pak), '--output', str(stage)], check=True)
    manifest = copy.deepcopy(original)
    manifest['name'] = NAME
    manifest['description'] = DESCRIPTION
    outfit = manifest['catalog']['outfits'][0]
    outfit['name'] = NAME
    outfit['description'] = DESCRIPTION
    outfit['variants'][0]['name'] = 'Black Pearl'
    # Save selections, remembered customization, presets and favorites use IDs.
    # Public labels and filenames can change without a state migration.
    assert outfit['id'] == original['id']
    assert outfit['variants'][0] == old_variant[0]
    for variant in outfit['variants']:
        package_path(variant['mesh'].split('.')[0])
    trio = out / STEM
    trio.mkdir()
    for suffix in ('.utoc', '.ucas'):
        name = STEM + suffix
        copy_verified(source_pak.with_suffix(suffix), trio / name)
        manifest['containers'][suffix]['file'] = name
    manifest_path = stage / PACKAGE_ROOT / original['id'] / 'manifest.json'
    atomic(manifest_path, manifest)
    for root in (stage, trio):
        for path in root.rglob('*'):
            output_path(path)
            component(path.name)
    subprocess.run([str(DEFAULT_REPAK), 'pack', str(stage), str(trio / (STEM + '.pak')), '--version', 'V8B'], check=True)
    assert verify(trio) == manifest
    reverse = copy.deepcopy(manifest)
    for key in ('name', 'description'):
        reverse[key] = original[key]
        reverse['catalog']['outfits'][0][key] = original['catalog']['outfits'][0][key]
    for suffix in ('.utoc', '.ucas'):
        reverse['containers'][suffix]['file'] = original['containers'][suffix]['file']
    assert reverse == original, 'Rename changed unrelated metadata'
    atomic(out / 'verification.json', dict(passed=True, source=str(source), output=str(trio),
        stable_id=manifest['id'], variant_id=old_variant[0]['id'], name=NAME,
        variant_name='Black Pearl', version=manifest['version'],
        hashes={p.name: sha(p) for p in trio.iterdir()},
        preserved='Cooked containers, thumbnail, dye resources, controls, palettes and all runtime asset paths.',
        pending='Install replacing the previous package; verify live label and saved appearance.'))
    print(trio)


if __name__ == '__main__':
    main()

"""Assemble the six-outfit candidate with an approved square thumbnail. Python 3.14."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT, digest, png_info
from css_package import verify


def strings(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for child in value.values():
            yield from strings(child)
    elif isinstance(value, list):
        for child in value:
            yield from strings(child)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--thumbnail', required=True, type=Path)
    parser.add_argument('--output', default='six-candidate1', choices=('six-candidate1', 'six-check1', 'six-v120'))
    parser.add_argument('--version', choices=('1.2.0',))
    args = parser.parse_args()
    thumbnail = png_info(args.thumbnail)
    work = ROOT / 'work/eve26'
    assets = work / 'six-assets1'
    proof = json.loads((assets / 'verification.json').read_text())
    for filename, expected in proof['files'].items():
        if digest(assets / filename) != expected:
            raise ValueError(f'Verified container changed: {filename}')
    source = next((work / 'six-meta1/metadata').rglob('manifest.json'))
    manifest = json.loads(source.read_text())
    if args.version:
        manifest['version'] = args.version
        manifest['catalog']['outfits'][0]['version'] = args.version
    variants = manifest['catalog']['outfits'][0]['variants']
    expected = {'black_pearl', 'prototype', 'skin_suit', 'bikini', 'casual_sweater', 'midsummer_alice'}
    if len(variants) != 6 or {v['id'] for v in variants} != expected:
        raise ValueError('Release variant scope differs')
    references = sorted({s for s in strings(variants) if s.startswith('/Game/')})
    for reference in references:
        package = reference.split('.')[0]
        path = 'MortalShell2/Content/' + package.removeprefix('/Game/') + '.uasset'
        if not package.startswith('/Game/CSS/') or path not in proof['assets']:
            raise ValueError(f'Missing CSS asset reference: {reference}')
    for variant in variants:
        if len(variant['customize'].get('palettes', [])) < 5:
            raise ValueError(f'Incomplete palettes: {variant["id"]}')
    output = work / args.output
    output.mkdir(exist_ok=False)
    metadata = output / 'metadata' / PACKAGE_ROOT / manifest['id']
    shutil.copytree(source.parent, metadata)
    shutil.copy2(args.thumbnail, metadata / 'thumbnail.png')
    manifest['thumbnail'] = thumbnail
    manifest['thumbnail_source'] = 'User-selected Eve mirror portrait'
    stem = 'CSS_EveStellarBlade_eins0fx_P'
    trio = output / stem
    trio.mkdir()
    for suffix in ('.utoc', '.ucas'):
        target = trio / (stem + suffix)
        shutil.copy2(assets / target.name, target)
        manifest['containers'][suffix] = dict(file=target.name, bytes=target.stat().st_size, sha256=digest(target))
    (metadata / 'manifest.json').write_text(json.dumps(manifest, separators=(',', ':')) + '\n')
    subprocess.run([str(DEFAULT_REPAK), 'pack', str(output / 'metadata'), str(trio / (stem + '.pak')),
                    '--version', 'V8B'], check=True)
    if verify(trio) != manifest:
        raise ValueError('Embedded metadata differs from staged metadata')
    (output / 'verification.json').write_text(json.dumps(dict(
        variants=[v['id'] for v in variants], references=references,
        files={p.name:digest(p) for p in trio.iterdir()}, thumbnail=thumbnail,
        scope='Candidate package only. Combined live review and release ZIP remain pending.'), indent=2)+'\n')
    print(trio)


if __name__ == '__main__':
    main()

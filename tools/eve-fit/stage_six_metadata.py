"""Stage six verified Eve variants and collision-free dye resources. Python 3.14.

This produces merge inputs only, not a releasable package.
"""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT, digest
from css_controls import validate, resource_info
from css_package import verify


def main():
    work = ROOT / 'work/eve26'
    audit = json.loads((work / 'six-audit.json').read_text())
    output = work / 'six-meta1'
    output.mkdir(exist_ok=False)
    manifest = copy.deepcopy(audit['sources'][0]['manifest'])
    outfit = manifest['catalog']['outfits'][0]
    outfit['variants'] = []
    manifest['resources'] = {}
    metadata = output / 'metadata' / PACKAGE_ROOT / manifest['id']
    metadata.mkdir(parents=True)
    report = []
    for row in audit['sources']:
        source = Path(row['source'])
        for name, expected in row['files'].items():
            if digest(source / name) != expected:
                raise ValueError(f'Source changed after audit: {source / name}')
        current = verify(source)
        if current != row['manifest']:
            raise ValueError(f'Metadata changed after audit: {source}')
        unpacked = output / row['label']
        subprocess.run([str(DEFAULT_REPAK), 'unpack', str(next(source.glob('*.pak'))),
                        '--output', str(unpacked)], check=True, stdout=subprocess.DEVNULL)
        resources = unpacked / PACKAGE_ROOT / current['id']
        if row['label'] == 'base':
            shutil.copy2(resources / 'thumbnail.png', metadata / 'thumbnail.png')
        for original in current['catalog']['outfits'][0]['variants']:
            if original['id'] not in row['selected_variants']:
                continue
            variant = copy.deepcopy(original)
            recipe = variant['customize']
            mapping = {}
            for filename in validate(recipe):
                if resource_info(resources / filename) != current['resources'][filename]:
                    raise ValueError(f'Resource differs: {filename}')
                renamed = f"dye-{row['label']}-{filename.removeprefix('dye-')}"
                shutil.copy2(resources / filename, metadata / renamed)
                manifest['resources'][renamed] = resource_info(metadata / renamed)
                mapping[filename] = renamed
            for surface in recipe.get('surfaces', []):
                surface['layers'] = {key: mapping[value] for key, value in surface['layers'].items()}
            if validate(recipe) != set(mapping.values()):
                raise ValueError('Incomplete dye resource remapping')
            for idle in variant.get('animations', {}).get('idle', []):
                if idle['id'] == 'eve':
                    idle['name'] = 'Eve Default Idle'
            outfit['variants'].append(variant)
            report.append(dict(id=variant['id'], name=variant['name'], resources=mapping,
                               controls=len(recipe['controls']), palettes=len(recipe.get('palettes', []))))
    assert len(outfit['variants']) == 6
    assert len({v['id'] for v in outfit['variants']}) == 6
    encoded = json.dumps(manifest, separators=(',', ':')) + '\n'
    if len(encoded.encode()) > 256 * 1024:
        raise ValueError('Combined manifest exceeds runtime size limit')
    (metadata / 'manifest.json').write_text(encoded)
    (output / 'report.json').write_text(json.dumps(dict(
        variants=report, manifest_bytes=len(encoded.encode()),
        pending=['Replace thumbnail with user-selected image', 'Build combined asset containers',
                 'Update container hashes', 'Review inherited Black Pearl templates for variant scope',
                 'Verify combined package and live outfit switching']), indent=2) + '\n')
    print(output)


if __name__ == '__main__':
    main()

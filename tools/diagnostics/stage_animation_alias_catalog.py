"""Stage legacy animation aliases using verified copies and pinned outfit manifests."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--metadata', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--verified-aliases', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    baseline = json.loads(args.baseline.read_text())
    verified = json.loads(args.verified_aliases.read_text())
    if verified['passed'] is not True:
        raise ValueError('Alias audit did not pass')
    for filename, checksum in {**verified['dependencies'], **verified['evidence']}.items():
        if sha(Path(filename)) != checksum:
            raise ValueError('Alias evidence changed: '+filename)
    manifest_relative = Path('MortalShell2/Content/CSS/Packages/css.locomotion/manifest.json')
    original = json.loads((args.metadata / manifest_relative).read_text())
    candidate = copy.deepcopy(original)
    entries = candidate['catalog']['animation_library']['entries']
    aliases = {row['alias']: row for row in verified['aliases']}
    if len(aliases) != len(verified['aliases']) or any(not row['identical'] for row in aliases.values()):
        raise ValueError('Invalid verified aliases')
    fixtures, added, inputs = [], [], {}
    for package in baseline['packages']:
        pinned = package.get('historical_manifest')
        if not pinned:
            continue
        path = Path(pinned['path'])
        if sha(path) != pinned['sha256']:
            raise ValueError('Outfit manifest changed: '+str(path))
        inputs[str(path)] = pinned['sha256']
        manifest = json.loads(path.read_text())
        for outfit in manifest['catalog'].get('outfits', []):
            for variant in outfit['variants']:
                animations = variant.get('animations', outfit.get('animations', {}))
                fixtures.append(dict(outfit=outfit['id'], variant=variant['id'], animations=animations,
                    skeleton=('/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2'
                              if outfit['id'] == 'eins0fx.unholygenessa'
                              else '/Game/CSS/Shared/SKEL_Base.SKEL_Base')))
                for slot, options in animations.items():
                    for option in options:
                        asset = option['clip' if slot == 'idle' else 'blend_space']
                        package_name, _, object_name = asset.partition('.')
                        recipe = aliases.get(package_name)
                        if recipe:
                            if recipe['object'] != object_name:
                                raise ValueError('Alias export name differs: '+asset)
                            canonical = recipe['source']+'.'+object_name
                        else:
                            canonical = asset
                        key = dict(slot=slot, id=option['id'], asset=canonical)
                        matches = [entry for entry in entries if key in entry.get('aliases', [])]
                        if len(matches) != 1 or matches[0].get('hide_weapons', False) != option.get('hide_weapons', False):
                            raise ValueError('No unique compatible library identity: '+asset)
                        alias = dict(slot=slot, id=option['id'], asset=asset)
                        if alias not in matches[0]['aliases']:
                            matches[0]['aliases'].append(alias)
                            added.append(dict(entry=matches[0]['id'], alias=alias))
    if len(fixtures) != 9 or len(added) != 22:
        raise ValueError(f'Unexpected current-outfit inventory: {len(fixtures)} variants, {len(added)} aliases')
    args.output.mkdir(parents=True)
    shutil.copytree(args.metadata, args.output / 'metadata')
    (args.output / 'metadata' / manifest_relative).write_text(json.dumps(candidate, separators=(',', ':'))+'\n')
    (args.output / 'variants.json').write_text(json.dumps(fixtures, indent=2)+'\n')
    inputs.update({str(p.resolve()): sha(p) for p in (args.baseline, args.verified_aliases, args.metadata / manifest_relative)})
    (args.output / 'receipt.json').write_text(json.dumps(dict(inputs=inputs, added=added,
        scope='Alias metadata only. No asset or skeleton migration; not installed.'), indent=2)+'\n')
    print(f'Staged {len(added)} aliases for {len(fixtures)} variants')


if __name__ == '__main__':
    main()

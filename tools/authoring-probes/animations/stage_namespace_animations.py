"""Stage pinned shared assets and explicit dependencies in independent private copies."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--source-content', type=Path, required=True)
    parser.add_argument('--dependency-content', type=Path, action='append', default=[],
                        help='Additional source roots for unpinned dependencies')
    parser.add_argument('--template', type=Path, required=True)
    parser.add_argument('--dependencies', type=Path, required=True)
    parser.add_argument('--native-reference-receipt', type=Path, required=True)
    parser.add_argument('--include-playback', action='store_true')
    parser.add_argument('--include-astral', action='store_true')
    parser.add_argument('--packages', type=Path, help='Explicit package list instead of animation selection')
    parser.add_argument('--mesh-snapshot-format', choices=('json-v1', 'array-blake3-v1'), default='json-v1',
                        help='Streaming array digests require the matching CSSRigAudit module')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    stage = args.output.resolve()
    if stage.exists() or not stage.resolve().is_relative_to((ROOT/'work/three-outfit-rig-physics').resolve()):
        raise ValueError('Use a new private rig workspace')
    source = args.source_content.resolve(strict=True)
    dependency_roots = [source, *(p.resolve(strict=True) for p in args.dependency_content)]
    inventory = json.loads(args.inventory.read_text())
    requested = None
    if args.packages:
        values = json.loads(args.packages.read_text())
        if not isinstance(values, list) or not values or any(not isinstance(p, str) for p in values):
            raise ValueError('Expected a nonempty package list')
        requested = set(values)
        if len(requested) != len(values) or args.include_playback or args.include_astral:
            raise ValueError('Explicit packages must be unique and cannot combine selection modes')
    assets = []
    for row in inventory['assets']:
        leaf = row['package'].rsplit('/', 1)[1]
        selected = row['package'] == '/Game/CSS/Shared/SKEL_Base' or (
            '/Shared/Locomotion/' in row['destination'] and leaf.startswith(('AN_', 'BS_')) and
            not row['cooked_alias_recipe'])
        selected = selected or (args.include_playback and
            row['package'].startswith('/Game/CSS/Locomotion/') and leaf.startswith(('ABP_', 'ACS_')))
        selected = selected or (args.include_astral and row['package'].startswith('/Game/CSS/SharedAssets/Astral/'))
        if requested is not None:
            selected = row['package'] in requested
        if not selected:
            continue
        if row['cooked_alias_recipe'] or not row['selected']:
            raise ValueError('Package requires a cooked recipe: '+row['package'])
        entry = row['selected']['identity']
        path = Path(entry['path']).resolve(strict=True)
        if sha(path) != entry['sha256']:
            raise ValueError('Selected source differs: '+str(path))
        assets.append(dict(old_package=row['package'], new_package=row['destination'],
                           source_file=str(path), source_sha256=entry['sha256']))
    if not assets or len({row['new_package'].casefold() for row in assets}) != len(assets):
        raise ValueError('No unique migration candidates')
    if requested is not None and {row['old_package'] for row in assets} != requested:
        raise ValueError('Explicit package is absent from pinned inventory')
    # Keep unrelated historical assets out of the registry. Rename loads
    # referring assets, so cloning the entire authoring tree is expensive.
    dependencies = json.loads(args.dependencies.read_text())
    packages = set(dependencies) | {row['old_package'] for row in assets}
    native_package = '/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton'
    receipt_path = args.native_reference_receipt.resolve(strict=True)
    receipt = json.loads(receipt_path.read_text())
    native_reference = receipt_path.parent/'game-reference/SKEL_Human_Skeleton.uasset'
    if (not receipt.get('passed') or receipt.get('skeleton') != native_package or
            sha(native_reference) != receipt['native_reference_sha256']):
        raise ValueError('Native reference does not match its verified binding receipt')
    if native_package not in packages:
        raise ValueError('Native animation dependency is missing')
    pinned = {row['package']: row['selected']['identity'] for row in inventory['assets']
              if row['selected'] and not row['cooked_alias_recipe']}
    stage.mkdir(parents=True)
    copied_inputs = []
    for package in sorted(packages):
        if not package.startswith('/Game/') or '..' in package.split('/') or '.' in package:
            raise ValueError('Invalid dependency package: '+package)
        relative = Path(package.removeprefix('/Game/')+'.uasset')
        original, copied = source/relative, stage/'Content'/relative
        if package == native_package:
            original = native_reference
        elif package in pinned:
            original = Path(pinned[package]['path']).resolve(strict=True)
            if sha(original) != pinned[package]['sha256']:
                raise ValueError('Pinned dependency changed: '+package)
        else:
            candidates = [root/relative for root in dependency_roots if (root/relative).is_file()]
            if not candidates or len({sha(p) for p in candidates}) != 1:
                raise ValueError('Missing or ambiguous unpinned dependency: '+package)
            original = candidates[0]
        copied.parent.mkdir(parents=True, exist_ok=True)
        same_device = original.stat().st_dev == copied.parent.stat().st_dev
        subprocess.run(['cp', '-a', '--reflink=always' if same_device else '--reflink=auto',
                        str(original), str(copied)], check=True)
        same_inode = (copied.stat().st_dev, copied.stat().st_ino) == (original.stat().st_dev, original.stat().st_ino)
        if copied.is_symlink() or same_inode or sha(copied) != sha(original):
            raise ValueError('Dependency copy is not independent: '+package)
        copied_inputs.append(dict(package=package, path=str(original), sha256=sha(original)))
    (stage/'copied-inputs.json').write_text(json.dumps(copied_inputs, indent=2)+'\n')
    for row in assets:
        copied = stage/'Content'/(row['old_package'].removeprefix('/Game/')+'.uasset')
        original = Path(row['source_file'])
        same_inode = (copied.stat().st_dev, copied.stat().st_ino) == (original.stat().st_dev, original.stat().st_ino)
        if copied.is_symlink() or same_inode or sha(copied) != row['source_sha256']:
            raise ValueError('Staging did not create a matching independent file')
    shutil.copy2(args.template/'NamespaceTrial.uproject', stage/'NamespaceTrial.uproject')
    (stage/'Binaries').symlink_to((args.template/'Binaries').resolve(), target_is_directory=True)
    for name in ('tmp', 'user', 'cache', 'ddc'):
        (stage/name).mkdir()
    refresh = [row['old_package'] for row in assets
               if row['old_package'].startswith('/Game/CSS/Eve/Anim/AN_')]
    plan = dict(assets=assets, refresh_packages=refresh, inventory=str(args.inventory.resolve()),
                mesh_snapshot_format=args.mesh_snapshot_format,
                inventory_sha256=sha(args.inventory),
                native_reference=dict(package=native_package, source=str(native_reference),
                    sha256=receipt['native_reference_sha256'], receipt=str(receipt_path),
                    receipt_sha256=sha(receipt_path)),
                scope='Shared sequences, blend spaces and common skeleton. '+
                    ('Includes outfit-owned playback blueprints and shared codec settings. ' if args.include_playback else
                     'Playback blueprints and codec settings remain separate migration steps. ')+
                    ('Includes Astral graphs, instances and neutral textures. ' if args.include_astral else '')+
                    'Historical aliases remain a separate migration step.')
    if requested is not None:
        plan['scope'] = 'Explicit pinned package migration; dependent packages remain separate unless listed.'
        plan['package_selection'] = dict(path=str(args.packages.resolve()), sha256=sha(args.packages))
    (stage/'input.json').write_text(json.dumps(plan, indent=2)+'\n')
    (stage/'recipe.py').write_bytes(Path(__file__).read_bytes())
    print(json.dumps(dict(stage=str(stage), assets=len(assets), refresh=len(refresh))))


if __name__ == '__main__':
    main()

"""Read UE 5.6.1 package dependencies and record existing legacy rig boundaries."""
import hashlib
import json
import os
from pathlib import Path

import unreal


def digest(path, algorithm='sha256'):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, algorithm).hexdigest()


root = Path(__file__).resolve().parents[3]
work = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
if not work.resolve().is_relative_to((root / 'work/cine-animation-port').resolve()):
    raise ValueError('Use the isolated animation workspace')
report = work / 'external-rig-assets.json'
manifest = work / 'external.txt'
if report.exists() or manifest.exists():
    raise FileExistsError('Dependency evidence already exists')
listing = work / 'cook.txt'
packages = [p.strip() for p in listing.read_text().splitlines()
            if p.strip() and not p.lstrip().startswith('#')]
if (not 1 <= len(packages) <= 64 or len(set(packages)) != len(packages) or
        any(not p.startswith('/Game/CSS/Locomotion/') or '.' in p for p in packages)):
    raise ValueError('Expected distinct shared-animation packages')
content = root.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False)
pending, edges, external = list(packages), {}, {}
protected = {str(listing): digest(listing)}
while pending:
    package = pending.pop()
    if package in edges:
        continue
    if len(edges) >= 4096:
        raise ValueError('Dependency graph exceeds inspection bound')
    source = content / (package.removeprefix('/Game/') + '.uasset')
    protected[str(source)] = digest(source)
    dependencies = registry.get_dependencies(package, options)
    if dependencies is None:
        raise ValueError('Dependency query failed: ' + package)
    edges[package] = sorted(map(str, dependencies))
    for dependency in edges[package]:
        if dependency.startswith('/Game/CSSAuthoring/'):
            if dependency in external:
                continue
            source = content / (dependency.removeprefix('/Game/') + '.uasset')
            checksum = digest(source)
            asset = unreal.load_asset(dependency)
            kind = asset.get_class().get_name() if asset else None
            if kind not in ('Skeleton', 'SkeletalMesh') or len(external) >= 64:
                raise ValueError('Unsupported external rig dependency: ' + dependency)
            external[dependency] = dict(kind=kind, sha1=digest(source, 'sha1'), sha256=checksum)
            protected[str(source)] = checksum
        elif dependency.startswith('/Game/CSS/'):
            pending.append(dependency)
if not external:
    raise ValueError('No external rig dependencies found')
if any(digest(Path(p)) != sha for p, sha in protected.items()):
    raise ValueError('An inspected source changed')
report.write_text(json.dumps(dict(complete=True, packages=packages, edges=edges,
    external=external, protected=protected,
    scope='Authoring hard/soft package boundary inspection only. Released dependency resolution and cooked readback are separate checks.'), indent=2) + '\n')
manifest.write_text(''.join(row['sha1'] + ' ' + name + '\n' for name, row in sorted(external.items())))
print('EXTERNAL_RIG_ASSETS', len(external), len(edges), flush=True)

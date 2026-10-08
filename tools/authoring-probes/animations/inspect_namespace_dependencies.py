"""UE registry-only dependency closure for an explicit migration batch."""
import json
import os
from pathlib import Path

import unreal

stage = Path(os.environ['CSS_NAMESPACE_STAGE']).resolve(strict=True)
output = stage/'dependencies.json'
if output.exists():
    raise FileExistsError(output)
packages = json.loads((stage/'dependency-roots.json').read_text())
if not packages or any(not p.startswith('/Game/') or '.' in p or '..' in p for p in packages):
    raise ValueError('Expected explicit Unreal package roots')
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
    include_hard_package_references=True, include_searchable_names=False,
    include_soft_management_references=False, include_hard_management_references=False)
pending, edges, assets, missing = list(packages), {}, {}, []
while pending:
    package = pending.pop()
    if package in edges:
        continue
    records = registry.get_assets_by_package_name(package, include_only_on_disk_assets=True,
                                                 skip_ar_filtered_assets=False)
    if not records:
        missing.append(package)
        edges[package] = []
        continue
    assets[package] = [dict(name=str(record.asset_name),
                           asset_class=str(record.asset_class_path.package_name)+'.'+
                                       str(record.asset_class_path.asset_name))
                       for record in records]
    dependencies = registry.get_dependencies(package, options)
    if dependencies is None:
        raise ValueError('Registry query failed: '+package)
    edges[package] = sorted(str(d) for d in dependencies)
    pending.extend(d for d in edges[package] if d.startswith('/Game/'))
output.write_text(json.dumps(dict(passed=not missing, roots=packages, edges=edges,
    assets=assets, missing_packages=sorted(missing),
    scope='Registry package dependencies, no asset loading or saving.'), indent=2)+'\n')
if missing:
    raise ValueError('Packages absent from the on-disk registry: '+', '.join(sorted(missing)))
print('CSS_NAMESPACE_DEPENDENCIES', len(edges), flush=True)

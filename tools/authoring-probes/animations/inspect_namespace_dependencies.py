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
pending, edges = list(packages), {}
while pending:
    package = pending.pop()
    if package in edges:
        continue
    dependencies = registry.get_dependencies(package, options)
    if dependencies is None:
        raise ValueError('Registry query failed: '+package)
    edges[package] = sorted(str(d) for d in dependencies)
    pending.extend(d for d in edges[package] if d.startswith('/Game/'))
output.write_text(json.dumps(dict(roots=packages, edges=edges,
    scope='Registry package dependencies, no asset loading or saving.'), indent=2)+'\n')
print('CSS_NAMESPACE_DEPENDENCIES', len(edges), flush=True)

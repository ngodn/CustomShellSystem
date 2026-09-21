"""Record hard/soft CSS package dependencies without modifying assets. UE 5.6.1."""
import hashlib
import json
import os
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(os.environ['CSS_CLOSURE_WORK']).resolve()
assert OUT.is_relative_to(ROOT / 'CustomShellSystem/work')
assert not (OUT / 'closure.json').exists()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False)
seed = '/Game/CSSAuthoring/DiagnosticReferences/SK_HeelSupportsV45C'
pending = [seed]
rows = {}
content = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
while pending:
    package = pending.pop()
    if package in rows:
        continue
    deps = sorted(str(p) for p in registry.get_dependencies(package, options))
    assets = registry.get_assets_by_package_name(package)
    file = content / (package.removeprefix('/Game/') + '.uasset')
    assert file.is_file(), package
    rows[package] = dict(dependencies=deps,
                        classes=sorted(str(a.asset_class_path.asset_name) for a in assets),
                        sha256=hashlib.sha256(file.read_bytes()).hexdigest())
    pending.extend(p for p in deps if p.startswith(('/Game/CSS/', '/Game/CSSAuthoring/')))
(OUT / 'closure.json').write_text(json.dumps(dict(seed=seed, packages=rows), indent=2)+'\n')
print('CSS_CLOSURE_PACKAGES', len(rows), flush=True)

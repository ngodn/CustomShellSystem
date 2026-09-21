"""Duplicate the audited texture map into short CSS packages. UE 5.6.1 Python."""
import hashlib
import json
import os
from pathlib import Path
import sys
import unreal

ROOT = Path(__file__).resolve().parents[3]
CSS = ROOT / 'CustomShellSystem'
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
sys.path.insert(0, str(CSS / 'tools'))
from css_paths import package_path, output_path
OUT = Path(os.environ['CSS_PATH_WORK']).resolve()
assert OUT.is_relative_to(CSS / 'work')
report_path = OUT / 'texture-copy.json'
assert not report_path.exists()
mapping = json.loads((OUT / 'textures.json').read_text())
assert len(mapping) == 90
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
protected = {}
for source, target in mapping.items():
    package_path(target)
    file = CONTENT / (source.removeprefix('/Game/') + '.uasset')
    destination = CONTENT / (target.removeprefix('/Game/') + '.uasset')
    output_path(destination)
    assert file.is_file() and not destination.exists()
    assert not unreal.EditorAssetLibrary.does_asset_exist(target)
    protected[str(file)] = sha(file)
(OUT / 'texture-protected.json').write_text(json.dumps(protected, indent=2)+'\n')
properties = ('srgb', 'compression_settings', 'lod_group', 'filter', 'address_x', 'address_y',
              'max_texture_size', 'never_stream', 'virtual_texture_streaming')
rows = []
for source, target in mapping.items():
    original = unreal.load_asset(source)
    assert isinstance(original, unreal.Texture2D)
    name, folder = target.rsplit('/', 1)[1], target.rsplit('/', 1)[0]
    duplicate = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, folder, original)
    assert duplicate and isinstance(duplicate, unreal.Texture2D)
    for key in properties:
        assert original.get_editor_property(key) == duplicate.get_editor_property(key), (target, key)
    assert unreal.EditorAssetLibrary.save_loaded_asset(duplicate, False)
    file = CONTENT / (target.removeprefix('/Game/') + '.uasset')
    rows.append(dict(source=source, package=target, sha256=sha(file)))
assert all(sha(Path(p)) == h for p, h in protected.items())
report_path.write_text(json.dumps(dict(passed=True, textures=rows, source_hashes=protected), indent=2)+'\n')
print('CSS_TEXTURE_COPY_PASS', len(rows), flush=True)

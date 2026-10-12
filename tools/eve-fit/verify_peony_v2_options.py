"""Reload and verify the seven saved finish textures in a fresh UE 5.6.1 process."""
import hashlib
import json
from pathlib import Path
import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
work = project.parent
assert work == Path('/mnt/eins0fxE/CSS-Development/peony-v2-options1')
assert not (work/'verified.json').exists()
record = json.loads((work/'authored.json').read_text())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


assert len(record['packages']) == 7
for path, expected in record['sources'].items():
    assert sha(Path(path)) == expected
for package, expected in record['packages'].items():
    source = project/'Content'/(package.removeprefix('/Game/')+'.uasset')
    assert sha(source) == expected
    asset = unreal.load_asset(package)
    assert isinstance(asset, unreal.Texture2D)
    assert asset.get_editor_property('srgb')
    assert asset.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_DEFAULT
    assert not asset.get_editor_property('virtual_texture_streaming')
    assert sha(source) == expected
(work/'verified.json').write_text(json.dumps(dict(passed=True, textures=7,
    authored_sha256=sha(work/'authored.json'), recipe_sha256=sha(Path(__file__))), indent=2)+'\n')

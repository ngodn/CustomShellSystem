"""Import the seven V2 finish textures missing from the character cook, UE 5.6.1."""
import json
from pathlib import Path
import sys
import unreal

workspace = Path(__file__).resolve().parents[3]
eve = workspace/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(eve/'tools'))
import author_peony_materials as common

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
work = project.parent
assert work == Path('/mnt/eins0fxE/CSS-Development/peony-v2-options1')
assert not (work/'authored.json').exists()
selection = json.loads((work/'selection.json').read_text())
plan = Path(selection['plan'])
assert common.sha(plan) == selection['sha256']
pending = json.loads(plan.read_text())['pending_import']
assert len(pending) == 7
common.ASSET_ROOT = '/Game/CSS/eins0fx/EveSB/PeonyV2'
packages, sources = {}, {str(plan): common.sha(plan)}
for path, row in pending.items():
    source = workspace/row['source']
    assert common.sha(source) == row['sha256']
    assert row['sampler'] == 'COLOR'
    package, name = path.split('.')
    assert not common.LIB.does_asset_exist(package)
    texture = common.import_texture(source, name)
    assert texture.get_path_name() == path
    packages[package] = common.sha(project/'Content'/(package.removeprefix('/Game/')+'.uasset'))
    sources[str(source)] = row['sha256']
assert all(common.sha(Path(p)) == value for p, value in sources.items())
(work/'authored.json').write_text(json.dumps(dict(packages=packages, sources=sources,
    recipe_sha256=common.sha(Path(__file__)), common_sha256=common.sha(Path(common.__file__)),
    engine=unreal.SystemLibrary.get_engine_version()), indent=2)+'\n')

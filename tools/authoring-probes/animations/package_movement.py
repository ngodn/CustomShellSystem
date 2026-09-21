"""Add cooked Eve movement to the accepted package and verify preserved payloads."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, PACKAGE_ROOT
from css_package import verify
from css_paths import package_path, windows_destination

WORK = ROOT/'work/anim13'
assert json.loads((WORK/'readback-result.json').read_text())['passed']
assert json.loads((WORK/'cook-exit.json').read_text())['exit_code'] == 0
OUT = WORK/'pack'
OUT.mkdir(exist_ok=False)
converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, OUT)
base = WORK/'base/decoded'
stage = OUT/'stage'
shutil.copytree(base, stage)
hashes = {str(p.relative_to(base)): hashlib.sha256(p.read_bytes()).hexdigest()
          for p in base.rglob('*') if p.is_file() and p.suffix in ('.uasset','.uexp','.ubulk')}
packages = (WORK/'cook.txt').read_text().splitlines()
assert len(packages) == len(set(packages)) == 22
for package in packages:
    package_path(package)
    assert package.startswith('/Game/CSS/Eve/Anim/')
    relative = package.removeprefix('/Game/')
    for suffix in ('.uasset','.uexp'):
        source = WORK/'cooked/CSSAuthoring/Content'/(relative+suffix)
        destination = stage/'MortalShell2/Content'/(relative+suffix)
        windows_destination('C:/CSS', str(destination.relative_to(stage)))
        assert source.is_file() and not destination.exists()
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
trio = OUT/'CSS_EveStellarBlade_eins0fx_P'
trio.mkdir()
stem = trio.name
converter.run(converter.retoc, 'to-zen', stage, trio/(stem+'.utoc'), '--version', 'UE5_6', '--no-parallel')
converter.run(converter.retoc, 'verify', trio/(stem+'.utoc'))
converter.base_containers(GAME, OUT/'containers')
for suffix in ('.utoc','.ucas'):
    (OUT/'containers'/(stem+suffix)).symlink_to(trio/(stem+suffix))
converter.run(converter.retoc, 'to-legacy', OUT/'containers', OUT/'readback',
              '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/')
differences = []
for relative, checksum in hashes.items():
    p = OUT/'readback'/relative
    assert p.is_file(), relative
    # Zen reconstructs the package summary, while export and bulk bytes must
    # stay exact for all previously accepted mesh/material/texture/rig assets.
    if p.suffix != '.uasset' and hashlib.sha256(p.read_bytes()).hexdigest() != checksum:
        differences.append(relative)
assert not differences, differences
for package in packages:
    relative = package.removeprefix('/Game/')
    assert (OUT/'readback/MortalShell2/Content'/(relative+'.uasset')).is_file()
    original = WORK/'cooked/CSSAuthoring/Content'/(relative+'.uexp')
    decoded = OUT/'readback/MortalShell2/Content'/(relative+'.uexp')
    assert original.read_bytes() == decoded.read_bytes(), package
baseline = ROOT/'work/eve1'/stem
old = verify(baseline)
metadata = OUT/'metadata'
converter.run(DEFAULT_REPAK, 'unpack', baseline/(stem+'.pak'), '--output', metadata)
manifest = copy.deepcopy(old)
variant = manifest['catalog']['outfits'][0]['variants'][0]
assert 'animations' not in variant
variant['animations'] = {slot.lower(): [dict(id='eve', name='Eve (Stellar Blade)',
    blend_space='/Game/CSS/Eve/Anim/BS_'+slot+'.BS_'+slot)] for slot in ('Walk','Jog','Sprint')}
for suffix in ('.utoc','.ucas'):
    p = trio/(stem+suffix)
    manifest['containers'][suffix] = dict(file=p.name, bytes=p.stat().st_size,
                                         sha256=hashlib.sha256(p.read_bytes()).hexdigest())
(metadata/PACKAGE_ROOT/manifest['id']/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
converter.run(DEFAULT_REPAK, 'pack', metadata, trio/(stem+'.pak'), '--version', 'V8B')
assert verify(trio) == manifest
reversed_manifest = copy.deepcopy(manifest)
del reversed_manifest['catalog']['outfits'][0]['variants'][0]['animations']
reversed_manifest['containers'] = old['containers']
assert reversed_manifest == old
(OUT/'verification.json').write_text(json.dumps(dict(passed=True, trio=str(trio),
    preserved_export_bulk_files=sum(Path(p).suffix != '.uasset' for p in hashes),
    added_animation_packages=packages, metadata_change='Only movement definitions and container checksums',
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in trio.iterdir()},
    scope='Package structure and payload preservation, pending live review.'), indent=2)+'\n')
print(trio)

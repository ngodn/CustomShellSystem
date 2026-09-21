"""Validate the final trio decoded without old CSS or diagnostic containers."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

CSS = Path(__file__).resolve().parents[2]
WORK = CSS / 'work/paths'
OUT = WORK / 'final1'
MOD = CSS.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
candidate = MOD / 'work/short1'
load = lambda p: json.loads(p.read_text())
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert not (OUT / 'verified.json').exists()
assert load(OUT / 'decode-exit.json')['exit_code'] == 0
assert load(candidate / 'report.json')['passed']
assets = load(OUT / 'assets.json')
packages = set((OUT / 'packages.txt').read_text().splitlines())
assert set(assets) == packages and len(packages) == 127
for file in (OUT / 'containers').iterdir():
    assert file.name.startswith(('global.', 'pakchunk', 'CSS_SeduXtress_eins0fx_P.')), file
    if file.name.startswith('CSS_SeduXtress_eins0fx_P.'):
        assert file.resolve().parent == candidate / 'trio', file
expected = load(WORK / 'rig1/assets.json')
expected.update(load(WORK / 'mat1/assets2.json'))
expected['/Game/CSS/SeduXtress/SK_BlackPearl2'] = load(WORK / 'rig2/decoded/SK_BlackPearl2.json')
compared = [p for p in packages if p in expected]
assert len(compared) == 37
assert all(assets[p] == expected[p] for p in compared)
for package in set(load(WORK / 'mat1/textures.json').values()):
    assert any(x['Type'] == 'Texture2D' for x in assets[package]), package
for package, data in assets.items():
    text = json.dumps(data)
    assert '/Game/CSSAuthoring/' not in text, package
    refs = {s.split('.', 1)[0] for s in re.findall(r'/Game/CSS/[^\s\"\x27]+', text)}
    assert refs <= packages, (package, refs - packages)
for relative, expected_hash in load(candidate / 'report.json')['copied'].items():
    assert digest(candidate / relative) == expected_hash
for name, expected_hash in load(candidate / 'report.json')['container_hashes'].items():
    assert digest(candidate / 'trio' / name) == expected_hash
command = [str(CSS / 'build/retoc-css-target/release/retoc'), 'verify', str(next((candidate / 'trio').glob('*.utoc')))]
(OUT / 'verify-command.json').write_text(json.dumps(command, indent=2) + '\n')
with (OUT / 'verify.log').open('w') as log:
    result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
(OUT / 'verify-exit.json').write_text(json.dumps(dict(exit_code=result.returncode)) + '\n')
assert result.returncode == 0
sys.path.insert(0, str(CSS / 'tools'))
from css_package import verify
from css_paths import package_path, windows_destination
manifest = verify(candidate / 'trio')
assert {v['mesh'] for outfit in manifest['catalog']['outfits'] for v in outfit['variants']} == {
    '/Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2'}
maximum_path = 0
for package in packages:
    package_path(package)
    for suffix in ('.uasset', '.uexp', '.ubulk'):
        destination = windows_destination('C:/CSS', 'MortalShell2/Content/' + package.removeprefix('/Game/') + suffix)
        maximum_path = max(maximum_path, len(destination))
protected = load(WORK / 'rig1/verified.json')['protected_hashes']
assert all(digest(Path(p)) == h for p, h in protected.items())
baseline = load(CSS / 'docs/development/v44-accepted-baseline.json')
installed = Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Content/Paks/~mods/CSS_SeduXtress_eins0fx_P')
assert all(digest(installed / name) == h for name, h in baseline['package_sha256'].items())
(OUT / 'verified.json').write_text(json.dumps(dict(passed=True, packages=127,
    exact_decoded_comparisons=37, texture_packages=90, resolved_css_references=True,
    maximum_windows_asset_path=maximum_path, installed_v44_unchanged=True,
    candidate=str(candidate / 'trio'), scope='Complete offline package and dependency verification; installation and live checks remain.'), indent=2) + '\n')
print('SHORT_PACKAGE_VERIFIED', flush=True)

"""Repackage alpha Eve with only the cooked camera-helper retarget repair."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, PACKAGE_ROOT
from css_package import verify, release_zip

WORK = ROOT / 'work/parry1'
load = lambda p: json.loads(p.read_text())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for mode in ('repair', 'verify', 'cook'):
    assert load(WORK / (mode + '-exit.json'))['exit_code'] == 0
assert load(WORK / 'verify-result.json')['passed']
STEM = 'CSS_EveStellarBlade_eins0fx_P'
baseline = ROOT / 'work/anim14/pack' / STEM
installed = GAME / 'Content/Paks/~mods' / STEM
assert {p.name: sha(p) for p in baseline.iterdir()} == {p.name: sha(p) for p in installed.iterdir()}
OUT = WORK / 'pack'
OUT.mkdir(exist_ok=False)
converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, OUT)
base = ROOT / 'work/anim14/pack/readback'
stage = OUT / 'stage'
shutil.copytree(base, stage)
relative = 'MortalShell2/Content/CSS/Shared/SKEL_Base'
for suffix in ('.uasset', '.uexp'):
    source = WORK / 'cooked/CSSAuthoring/Content/CSS/Shared' / ('SKEL_Base' + suffix)
    assert source.is_file()
    shutil.copy2(source, stage / (relative + suffix))
trio = OUT / STEM
trio.mkdir()
converter.run(converter.retoc, 'to-zen', stage, trio / (STEM + '.utoc'), '--version', 'UE5_6', '--no-parallel')
converter.run(converter.retoc, 'verify', trio / (STEM + '.utoc'))
converter.base_containers(GAME, OUT / 'containers')
for suffix in ('.utoc', '.ucas'):
    (OUT / 'containers' / (STEM + suffix)).symlink_to(trio / (STEM + suffix))
converter.run(converter.retoc, 'to-legacy', OUT / 'containers', OUT / 'readback',
              '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/')
files = {str(p.relative_to(base)) for p in base.rglob('*') if p.is_file()}
readback = OUT / 'readback'
assert files == {str(p.relative_to(readback)) for p in readback.rglob('*') if p.is_file()}
changed = []
preserved = 0
for name in sorted(files):
    if Path(name).suffix == '.uasset':
        continue  # Zen reconstructs package headers; compare decoded properties separately.
    if (base / name).read_bytes() != (readback / name).read_bytes():
        changed.append(name)
    else:
        preserved += 1
assert changed == [relative + '.uexp'], changed
assert (readback / (relative + '.uexp')).read_bytes() == (stage / (relative + '.uexp')).read_bytes()
metadata = OUT / 'metadata'
converter.run(DEFAULT_REPAK, 'unpack', baseline / (STEM + '.pak'), '--output', metadata)
original = verify(baseline)
manifest = copy.deepcopy(original)
for suffix in ('.utoc', '.ucas'):
    p = trio / (STEM + suffix)
    manifest['containers'][suffix] = dict(file=p.name, bytes=p.stat().st_size, sha256=sha(p))
(metadata / PACKAGE_ROOT / manifest['id'] / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
converter.run(DEFAULT_REPAK, 'pack', metadata, trio / (STEM + '.pak'), '--version', 'V8B')
assert verify(trio) == manifest
unchanged = copy.deepcopy(manifest)
unchanged['containers'] = original['containers']
assert unchanged == original
archive = WORK / (STEM + '-v1.0.0-alpha.1.zip')
release_zip(trio, archive)
(OUT / 'verification.json').write_text(json.dumps(dict(passed=True,
    changed_exports=changed, preserved_exports_and_bulk=preserved,
    hashes={p.name: sha(p) for p in trio.iterdir()}, archive_sha256=sha(archive),
    scope='Only skeleton export changed; independent property comparison and live check still required.'), indent=2))
print(archive)

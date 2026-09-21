"""Add accepted grounding metadata while preserving every cooked asset. Python 3.14."""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

CSS=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(CSS/'tools'))
from css import atomic, sha, copy_verified
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT
from css_package import verify

mod=CSS.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
old=mod/'work/short1';out=mod/'work/ground1'
assert not out.exists();out.mkdir()
original=verify(old/'trio')
trio=out/'trio';trio.mkdir()
pak=next((old/'trio').glob('*.pak'))
stage=out/'stage'
subprocess.run([str(DEFAULT_REPAK),'unpack',str(pak),'--output',str(stage)],check=True)
manifest_file=stage/PACKAGE_ROOT/original['id']/'manifest.json'
manifest=json.loads(manifest_file.read_text());assert manifest==original
variants=manifest['catalog']['outfits'][0]['variants'];assert len(variants)==1
assert variants[0]['mesh']=='/Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2'
assert 'ground_offset_cm' not in variants[0]
variants[0]['ground_offset_cm']=-3
atomic(manifest_file,manifest)
for suffix in ('.ucas','.utoc'):
    copy_verified(pak.with_suffix(suffix),trio/pak.with_suffix(suffix).name)
subprocess.run([str(DEFAULT_REPAK),'pack',str(stage),str(trio/pak.name),'--version','V8B'],check=True)
verified=verify(trio)
reverse=copy.deepcopy(verified);assert reverse['catalog']['outfits'][0]['variants'][0].pop('ground_offset_cm')==-3
assert reverse==original
readback=out/'readback'
subprocess.run([str(DEFAULT_REPAK),'unpack',str(trio/pak.name),'--output',str(readback)],check=True)
files=lambda root:{str(p.relative_to(root)):sha(p) for p in root.rglob('*') if p.is_file()}
assert files(stage)==files(readback)
shutil.copytree(old/'metadata',out/'metadata')
catalog=json.loads((out/'metadata/catalog.json').read_text())
assert len(catalog['outfits'][0]['variants'])==1
catalog['outfits'][0]['variants'][0]['ground_offset_cm']=-3
atomic(out/'metadata/catalog.json',catalog)
atomic(out/'verification.json',dict(passed=True,offset_cm=-3,
    source=str(old/'trio'),trio=str(trio),hashes=files(trio),
    scope='Only variant ground_offset_cm added. Cooked containers copied byte-for-byte; complete manifest reverses exactly and packed resources match staging.'))
print('Prepared',trio)

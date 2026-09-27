"""Restore Black Pearl motion controls without modifying the accepted fitted assets."""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(repo/'tools'))
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT, digest
from css_package import verify

work = repo/'work/eve26'
original = work/'p14trial'
output = work/'p14motion'
assert not output.exists()
stem = 'CSS_EveFit14_P'
manifest = verify(original/stem)
source = json.loads((work/'p13meta'/PACKAGE_ROOT/'eins0fx.seduxtress/manifest.json').read_text())
black = next(v for v in source['catalog']['outfits'][0]['variants'] if v['id']=='black_pearl')
controls = [copy.deepcopy(c) for c in black['customize']['controls'] if c['kind']=='rig']
assert {c['id'] for c in controls} == {'hair_motion','chest_motion','glute_motion','thigh_motion','belly_motion'}
repaired = copy.deepcopy(manifest)
variant = repaired['catalog']['outfits'][0]['variants'][0]
assert not any(c['kind']=='rig' for c in variant['customize']['controls'])
variant['customize']['controls'].extend(controls)
check = copy.deepcopy(repaired)
check['catalog']['outfits'][0]['variants'][0]['customize']['controls'] = manifest['catalog']['outfits'][0]['variants'][0]['customize']['controls']
assert check == manifest
output.mkdir()
shutil.copytree(original/'metadata',output/'metadata')
path = output/'metadata'/PACKAGE_ROOT/manifest['id']/'manifest.json'
path.write_text(json.dumps(repaired,separators=(',',':'))+'\n')
trio = output/stem
trio.mkdir()
for suffix in ('.utoc','.ucas'):
    shutil.copy2(original/stem/(stem+suffix),trio/(stem+suffix))
    assert digest(trio/(stem+suffix)) == digest(original/stem/(stem+suffix))
with (output/'pack.log').open('x') as log:
    subprocess.run([str(DEFAULT_REPAK),'pack',str(output/'metadata'),str(trio/(stem+'.pak')),'--version','V8B'],check=True,stdout=log,stderr=subprocess.STDOUT)
assert verify(trio) == repaired
(output/'verification.json').write_text(json.dumps(dict(controls=controls,
    files={p.name:digest(p) for p in trio.iterdir()},assets_byte_identical=True,
    only_manifest_controls_changed=True,
    scope='Packaging verified; live body motion still needs review.'),indent=2)+'\n')
print(trio)

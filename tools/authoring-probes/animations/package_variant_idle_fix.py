"""Replace only the two repaired graphs in the verified E6 candidate. Python 3.14."""
import json
import os
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'tools'))
from css_convert import Converter, DEFAULT_REPAK, asset_info, digest
from css_package import verify
from convert_beaute import DEFAULT_GAME

work = ROOT/'work/eve-idle1'
old = work/'pack2'
proof = json.loads((old/'verification.json').read_text())
stem = 'CSS_EveStellarBlade_eins0fx_P'
assert all(digest(old/stem/n)==h for n,h in proof['files'].items())
output = work/'pack3'
output.mkdir(exist_ok=False)
converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
stage = output/'stage'
shutil.copytree(old/'readback', stage, copy_function=os.link)
replaced = {'MortalShell2/Content/CSS/EveTest/'+n+'.uasset' for n in ('ABP_BikiniFeet2','ABP_KnitFeet1')}
expected = set(proof['preserved_assets']) | set(proof['added_assets'])
assert {str(p.relative_to(stage)) for p in stage.rglob('*.uasset')} == expected
for path in replaced:
    relative = path.removeprefix('MortalShell2/Content/')
    for suffix in ('.uasset', '.uexp'):
        target = (stage/path).with_suffix(suffix)
        target.unlink()  # Break the hard link before replacing this graph.
        shutil.copy2((work/'cook3/CSSAuthoring/Content'/relative).with_suffix(suffix), target)
trio = output/stem
trio.mkdir()
target = trio/(stem+'.utoc')
converter.run(converter.retoc,'to-zen',stage,target,'--version','UE5_6','--no-parallel')
converter.run(converter.retoc,'verify',target)
converter.base_containers(DEFAULT_GAME,output/'checks')
for suffix in ('.utoc','.ucas'):
    (output/'checks'/(stem+suffix)).symlink_to(trio/(stem+suffix))
decoded = output/'readback'
converter.run(converter.retoc,'to-legacy',output/'checks',decoded,
              '--version','UE5_6','--no-parallel','--no-shaders','-f','/CSS/')
assert {str(p.relative_to(decoded)) for p in decoded.rglob('*.uasset')} == expected
for path in expected:
    before = (stage if path in replaced else old/'readback')/path
    after = decoded/path
    a,b = asset_info(before.read_bytes()),asset_info(after.read_bytes())
    if path not in replaced:
        assert a == b, path
    else:
        assert a['package']==b['package'] and len(a['exports'])==len(b['exports'])
        for x,y in zip(a['exports'],b['exports'],strict=True):
            assert {k:v for k,v in x.items() if k!='asset'}=={k:v for k,v in y.items() if k!='asset'},path
    for suffix in ('.uexp','.ubulk','.uptnl'):
        a,b=before.with_suffix(suffix),after.with_suffix(suffix)
        assert a.exists()==b.exists()
        assert not a.exists() or digest(a)==digest(b),str(a)
shutil.copytree(old/'metadata',output/'metadata')
manifest_path=next((output/'metadata').rglob('manifest.json'))
manifest=json.loads(manifest_path.read_text())
for suffix in ('.utoc','.ucas'):
    p=trio/(stem+suffix)
    manifest['containers'][suffix]=dict(file=p.name,bytes=p.stat().st_size,sha256=digest(p))
manifest_path.write_text(json.dumps(manifest,separators=(',',':'))+'\n')
converter.run(DEFAULT_REPAK,'pack',output/'metadata',trio/(stem+'.pak'),'--version','V8B')
assert verify(trio)==manifest
(output/'verification.json').write_text(json.dumps(dict(replaced=sorted(replaced),
    preserved_assets=sorted(expected-replaced),files={p.name:digest(p) for p in trio.iterdir()},
    scope='Two animation graph replacements; all other decoded assets preserved; runtime acceptance pending'),indent=2)+'\n')
print(trio)

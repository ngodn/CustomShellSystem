"""Relocate native boss meshes without changing render or clothing payloads."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
CSS = Path('/home/eins0fx/development/mods/msII/CustomShellSystem')
ROOT = CSS / 'work/npc-appearances/bosses'
sys.path.insert(0, str(CSS / 'tools'))
from css_convert import Converter, DEFAULT_REPAK, replace_references
from css import GAME
plans = json.loads((ROOT / 'selected-plan.json').read_text())
assert json.loads((ROOT / 'cook-exit.json').read_text())['exit_code'] == 0
out = ROOT / 'isolated-package1'
out.mkdir(exist_ok=False)
stage = out / 'stage'
source = ROOT / 'native-meshes/legacy'
cook = CSS / 'work/boss-rigs1/CSSAuthoring/Content'
mapping = {p[a]: p[b] for p in plans for a,b in [('source_mesh','mesh'),('source_skeleton','skeleton')]}
records = []
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for plan in plans:
    header = source / 'MortalShell2/Content' / (plan['source_mesh'][6:] + '.uasset')
    assert header.is_file()
    for path in sorted(header.parent.glob(header.stem + '.*')):
        assert path.suffix in ('.uasset','.uexp','.ubulk','.uptnl')
        original = path.read_bytes()
        edits = []
        data, count = replace_references(original, mapping, edits)
        undo = bytearray(data)
        for offset, old in edits: undo[offset:offset+len(old)] = old
        assert bytes(undo) == original
        target = stage / 'MortalShell2/Content' / (plan['mesh'][6:] + path.suffix)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        records.append(dict(source=str(path),source_sha256=sha(path),target=str(target.relative_to(stage)),sha256=sha(target),replacements=count))
    header = cook / (plan['skeleton'][6:] + '.uasset')
    assert header.is_file()
    for path in sorted(header.parent.glob(header.stem + '.*')):
        assert path.suffix in ('.uasset','.uexp','.ubulk','.uptnl')
        target = stage / 'MortalShell2/Content' / path.relative_to(cook)
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,target)
        records.append(dict(source=str(path),source_sha256=sha(path),target=str(target.relative_to(stage)),sha256=sha(target)))
shutil.copy2(source/'scriptobjects.bin',stage/'scriptobjects.bin')
(out/'payloads.json').write_text(json.dumps(records,indent=2))
converter=Converter(CSS/'build/retoc-css-target/release/retoc',DEFAULT_REPAK,out)
toc=out/'CSS_BossAppearancesTrial_P.utoc'
converter.run(converter.retoc,'to-zen',stage,toc,'--version','UE5_6','--no-parallel')
converter.run(converter.retoc,'verify',toc)
mount=out/'containers'
converter.base_containers(GAME,mount)
for suffix in ('.pak','.utoc','.ucas'):
    path=toc.with_suffix(suffix)
    (mount/path.name).symlink_to(path)
packages=[p[key] for p in plans for key in ('mesh','skeleton')]
(out/'packages.txt').write_text('\n'.join(packages)+'\n')
(out/'meshes.txt').write_text('\n'.join(p['mesh'] for p in plans)+'\n')
(out/'references.txt').write_text('\n'.join(p+'.'+p.rsplit('/',1)[1] for p in packages)+'\n')
dotnet='/home/eins0fx/.local/share/mise/installs/dotnet/10.0.401/dotnet'
mappings=GAME/'Binaries/Win64/ue4ss/MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241+1339-d7e7826d.usmap'
for name,args in {
    'decoded':[str(CSS/'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'),str(mount),str(mappings),str(out/'packages.txt'),str(out/'decoded.json')],
    'mesh-digests':[str(CSS/'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'),str(mount),str(mappings),str(out/'meshes.txt'),str(out/'mesh-digests.json'),'--mesh-digests'],
    'imports':[str(CSS/'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll'),str(mount),str(mappings),str(out/'packages.txt'),str(out/'references.txt'),str(out/'imports.json')],
}.items():
    command=[dotnet,*args]
    (out/(name+'-command.json')).write_text(json.dumps(command,indent=2))
    with (out/(name+'.log')).open('w') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT)
    print(name,r.returncode,flush=True)
    r.check_returncode()
assert json.loads((out/'imports.json').read_text())['passed']
(out/'package.json').write_text(json.dumps(dict(passed=True,installed=False,files={p.name:sha(p) for p in [toc,toc.with_suffix('.ucas'),toc.with_suffix('.pak')]},scope='Container and imports; visual acceptance pending'),indent=2))

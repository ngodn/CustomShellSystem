"""Pack and independently decode the isolated B2 cook without deployment."""
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
COOK=Path(os.environ.get('CSS_B2_COOK_DIR',str(WORK/'arm-rest-b2-cook-v1'))).resolve()
OUT=Path(os.environ.get('CSS_B2_COOK_READBACK_DIR',str(WORK/'arm-rest-b2-cooked-readback-v1'))).resolve()
assert COOK.parent==OUT.parent==WORK.resolve()
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert load(COOK/'exit.json')['exit_code']==0
manifest=load(COOK/'input.json')
assert all(digest(Path(p))==h for p,h in manifest['protected_hashes'].items())
assert not OUT.exists()
OUT.mkdir()

def run(label,args,env=None):
    (OUT/(label+'.command.json')).write_text(json.dumps(args,indent=2)+'\n')
    with (OUT/(label+'.log')).open('w') as log:
        result=subprocess.run(args,env=env,stdout=log,stderr=subprocess.STDOUT)
    (OUT/(label+'.exit.json')).write_text(json.dumps({'exit_code':result.returncode})+'\n')
    assert result.returncode==0,(label,result.returncode)

stage=OUT/'stage'
copied={}
for package in manifest['packages']:
    assert package.startswith(('/Game/CSSAuthoring/','/Game/CSS/'))
    relative=package.removeprefix('/Game/')
    for suffix in ('.uasset','.uexp','.ubulk'):
        source=COOK/'cooked/CSSAuthoring/Content'/(relative+suffix)
        if suffix=='.uasset':assert source.is_file(),source
        if not source.exists():continue
        dest=stage/'MortalShell2/Content'/(relative+suffix)
        dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,dest);assert digest(source)==digest(dest)
        copied[str(source)]=digest(source)
shutil.copy2(COOK/'cooked/CSSAuthoring/Metadata/scriptobjects.bin',stage/'scriptobjects.bin')
retoc=ROOT/'CustomShellSystem/build/retoc-css-target/release/retoc'
utoc=OUT/'ArmRestB2Diagnostic_P.utoc'
run('pack',[str(retoc),'to-zen',str(stage),str(utoc),'--version','UE5_6','--no-parallel'])
run('verify',[str(retoc),'verify',str(utoc)])
containers=OUT/'containers';containers.mkdir()
for source in (MOD/'work/cooked-readback-hand-v43/containers').iterdir():
    if source.suffix in ('.utoc','.ucas','.pak'):(containers/source.name).symlink_to(source.resolve())
for source in OUT.glob('ArmRestB2Diagnostic_P.*'):(containers/source.name).symlink_to(source)
for name in ('tmp','dotnet-home','cache'):(OUT/name).mkdir()
dotnet=Path('/home/eins0fx/.local/share/mise/installs/dotnet/10.0.401')
exporter=ROOT/'CSS-eins0fx-collections/shibari-eins0fx-CSS/tools/SBMeshExport/bin/Release/net10.0/SBMeshExport.dll'
mapping=Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Binaries/Win64/ue4ss/MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241+1339-d7e7826d.usmap')
args=['bwrap','--ro-bind','/','/','--bind',str(ROOT),str(ROOT),'--bind',str(OUT/'tmp'),'/tmp','--dev-bind','/dev','/dev','--die-with-parent','--',str(dotnet/'dotnet'),str(exporter),'GAME_UE5_6',str(containers),str(mapping),str(OUT/'decoded'),*manifest['packages']]
env=dict(os.environ,TMPDIR=str(OUT/'tmp'),DOTNET_ROOT=str(dotnet),DOTNET_CLI_HOME=str(OUT/'dotnet-home'),SBMESH_FORMAT='psk',XDG_CACHE_HOME=str(OUT/'cache'))
run('decode',args,env)
results=load(OUT/'decoded/export-results.json')
assert results and all(r['Success'] for r in results),results
assert all((OUT/'decoded'/(p.rsplit('/',1)[1]+'.json')).is_file() for p in manifest['packages'])
assert all(digest(Path(p))==h for p,h in manifest['protected_hashes'].items())
(OUT/'report.json').write_text(json.dumps(dict(passed=True,scope='Independent package decode only; geometry/morph comparisons and actual cooked execution are separate.',packages=manifest['packages'],cooked_hashes=copied,container_hashes={p.name:digest(p) for p in OUT.glob('ArmRestB2Diagnostic_P.*')},tool_hashes={str(p):digest(p) for p in (retoc,exporter,mapping)},protected_hashes=manifest['protected_hashes']),indent=2)+'\n')
print('B2 diagnostic package decoded',OUT)

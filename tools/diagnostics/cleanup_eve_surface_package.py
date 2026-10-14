"""Remove verified duplicate staging from the completed offline Eve assembly."""
import json
import os
from pathlib import Path
import shutil
import subprocess

from assemble_eve_surface_package import ROOT, OUT, LINK, read, write, require, unchanged
from check_import_cooked_mesh import sha
from css_convert import asset_info


def main():
    require(OUT.resolve()==OUT and LINK.is_symlink() and LINK.resolve()==OUT,'Output location changed')
    require(not (OUT/'cleanup-completed.json').exists(),'Cleanup already recorded')
    proof=read(OUT/'verified.json');plan=read(OUT/'plan.json')
    require(proof['passed'] and proof['plan_sha256']==sha(OUT/'plan.json'),'Unverified package')
    unchanged(plan['protected'])
    for step in ('catalog','imports','roundtrip','meshes'):
        require(read(OUT/(step+'-exit.json'))==dict(returncode=0),'Verification step is not terminal')
    for p in Path('/proc').iterdir():
        if not p.name.isdigit() or int(p.name)==os.getpid():continue
        try:args=(p/'cmdline').read_bytes().split(b'\0')
        except (FileNotFoundError,PermissionError,ProcessLookupError):continue
        tokens=[a.decode(errors='replace') for a in args if a]
        running_assembly=any(t.endswith('/assemble_eve_surface_package.py') or t=='tools/diagnostics/assemble_eve_surface_package.py' for t in tokens)
        running_artifact=any(str(OUT) in t for t in tokens) and any(Path(t).name in ('retoc','repak','AssetReadback.dll','AssetDependencyAudit.dll','css_animation_library_tests') for t in tokens)
        require(not running_assembly and not running_artifact,'Assembly or artifact reader still active: '+p.name)
    retained={str(OUT/'plan.json'):sha(OUT/'plan.json'),str(OUT/'verified.json'):sha(OUT/'verified.json')}
    for relative,digest in proof['files'].items():
        path=OUT/'pack'/relative;require(sha(path)==digest,'Package changed');retained[str(path)]=digest
    for relative,row in plan['payloads'].items():
        source=Path(row['path']);staged=OUT/'stage'/relative;roundtrip=OUT/'roundtrip'/relative
        require(sha(source)==row['sha256'] and sha(staged)==row['sha256'],'Payload source/stage changed')
        retained[str(source)]=row['sha256']
        if roundtrip.suffix=='.uasset':require(asset_info(roundtrip.read_bytes())['package']==asset_info(source.read_bytes())['package'],'Readback identity changed')
        else:require(sha(roundtrip)==row['sha256'],'Readback export/bulk changed')
    auxiliary=read(OUT/'roundtrip-auxiliary.json')
    require(auxiliary['path']=='scriptobjects.bin','Unexpected auxiliary output')
    path=OUT/'roundtrip'/auxiliary['path']
    require(sha(path)==auxiliary['sha256'] and path.stat().st_size==auxiliary['bytes'],'Global script table changed')
    require({str(p.relative_to(OUT/'roundtrip')) for p in (OUT/'roundtrip').rglob('*') if p.is_file()}==set(plan['payloads'])|{'scriptobjects.bin'},'Unexpected roundtrip files')
    retained[str(OUT/'roundtrip-auxiliary.json')]=sha(OUT/'roundtrip-auxiliary.json')
    for p in (OUT/'metadata').rglob('*'):
        if p.is_file():
            digest=sha(p);require(sha(OUT/'metadata-readback'/p.relative_to(OUT/'metadata'))==digest,'Metadata differs')
            retained[str(p)]=digest
    paths=[OUT/n for n in ('stage','roundtrip','metadata-readback')];groups=[]
    for p in paths:
        require(p.is_dir() and not p.is_symlink() and p.resolve().is_relative_to(OUT),'Deletion escapes task directory')
        files=[f for f in p.rglob('*') if f.is_file() and not f.is_symlink()]
        groups.append(dict(path=str(p),files=len(files),logical_bytes=sum(f.stat().st_size for f in files)))
    prelist=ROOT/'docs/development/cleanup-2026-10-14-eve-surface-package.md'
    df=['df','-h','/','/run/media/eins0fx/theark']
    removal=dict(groups=groups,retained=retained,prelist_sha256=sha(prelist),recipe_sha256=sha(Path(__file__)),
                 before=subprocess.check_output(df,text=True))
    write(OUT/'cleanup-plan.json',removal)
    for p in paths:shutil.rmtree(p)
    unchanged(retained)
    result=dict(removal,completed=True,after=subprocess.check_output(df,text=True))
    write(OUT/'cleanup-completed.json',result)
    print(sum(g['logical_bytes'] for g in groups),'logical bytes removed')
    print(result['before'],result['after'])


if __name__=='__main__':main()

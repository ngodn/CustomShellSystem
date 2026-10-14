"""Build the isolated C++20 cloth-preservation commandlet with UE 5.6.1."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
WORK = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/cloth-transfer').resolve()
ENGINE = ROOT.parent/'CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resume-build',action='store_true')
    parser.add_argument('--rebuild',action='store_true')
    args = parser.parse_args()
    if args.resume_build and args.rebuild:
        raise ValueError('Choose resume or rebuild')
    if args.resume_build or args.rebuild:
        if args.resume_build and json.loads((WORK/'build-exit.json').read_text())['returncode']==0:
            raise ValueError('Only resume a failed build')
        history = WORK/('previous-builds' if args.rebuild else 'failed-builds')
        history.mkdir(exist_ok=True)
        attempt = history/str(len(list(history.iterdir()))+1)
        attempt.mkdir()
        for name in ('build-exit.json','build.log','build.engine.log','build-inputs.json','module.json'):
            if (WORK/name).exists():
                shutil.move(WORK/name,attempt/name)
        shutil.copytree(WORK/'editor/Source',attempt/'Source')
        if args.rebuild:
            shutil.copytree(WORK/'editor/Binaries',attempt/'Binaries')
    else:
        WORK.mkdir(exist_ok=False)
    editor = WORK/'editor'
    module = editor/'Source/CSSAuthoring'
    module.mkdir(parents=True,exist_ok=True)
    version = json.loads((ENGINE/'Build/Build.version').read_text())
    if [version[k] for k in ('MajorVersion','MinorVersion','PatchVersion','Changelist')]!=[5,6,1,44394996]:
        raise ValueError('Unexpected engine build')
    protected = {str(Path(__file__)):sha(Path(__file__)),str(ENGINE/'Build/Build.version'):sha(ENGINE/'Build/Build.version')}
    sources = [(p,module/p.name) for p in (ROOT/'tools/authoring/cloth-transfer').iterdir() if p.is_file()]
    sources.append((ROOT/'tools/authoring/CSSAuthoring/Source/CSSAuthoringEditor.Target.cs',editor/'Source/CSSAuthoringEditor.Target.cs'))
    descriptor = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work/peony-character1/editor/CSSAuthoring.uproject'
    sources.append((descriptor,editor/'CSSAuthoring.uproject'))
    for source,target in sources:
        shutil.copy2(source,target)
        protected[str(source)] = protected[str(target)] = sha(source)
    for name in ('ubt-config','ubt-cache','dotnet','ubt-runtime-scratch','tmp','ddc','user'):
        (WORK/name).mkdir(exist_ok=True)
    env = dict(os.environ,TMPDIR=str(WORK/'tmp'),DOTNET_CLI_HOME=str(WORK/'dotnet'),
               XDG_CONFIG_HOME=str(WORK/'ubt-config'),XDG_CACHE_HOME=str(WORK/'ubt-cache'))
    command = ['nice','-n','19','bwrap','--ro-bind','/','/','--bind',str(WORK),str(WORK),
        '--bind',str(WORK/'ubt-runtime-scratch'),'/tmp','--dev-bind','/dev','/dev','--die-with-parent','--',
        str(ENGINE/'Binaries/ThirdParty/DotNet/8.0.300/linux-x64/dotnet'),
        str(ENGINE/'Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll'),
        'CSSAuthoringEditor','Linux','Development','-Project='+str(editor/'CSSAuthoring.uproject'),
        '-NoHotReloadFromIDE','-NoUBA','-MaxParallelActions=2','-Log='+str(WORK/'build.engine.log')]
    (WORK/'build-inputs.json').write_text(json.dumps(protected,indent=2)+'\n')
    with (WORK/'build.log').open('w') as log:
        result = subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT)
    changed = [p for p,h in protected.items() if sha(Path(p))!=h]
    (WORK/'build-exit.json').write_text(json.dumps(dict(command=command,returncode=result.returncode,
        changed_inputs=changed,protected=protected),indent=2)+'\n')
    print(dict(returncode=result.returncode,changed_inputs=changed),flush=True)
    if result.returncode or changed:
        raise SystemExit(1)
    names = ('CSSAuthoring.uproject','Binaries/Linux/UnrealEditor.modules','Binaries/Linux/libUnrealEditor-CSSAuthoring.so')
    (WORK/'module.json').write_text(json.dumps({name:sha(editor/name) for name in names},indent=2)+'\n')


if __name__=='__main__':
    main()

"""Run one Holiday authoring commandlet with workspace-local scratch and caches."""
import argparse
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[3]
work=root/'CustomShellSystem/work/eve26'
engine=root/'CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine'
project=root/'CSS-eins0fx-collections/tools/CSSAuthoring/CSSAuthoring.uproject'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--log',required=True)
p.add_argument('args',nargs=argparse.REMAINDER)
a=p.parse_args()
log=work/a.log
assert log.parent==work and not log.exists(),log
args=a.args[1:] if a.args and a.args[0]=='--' else a.args
assert args
assert any(arg.lower().startswith('-run=') for arg in args), 'Use commandlet mode, including -run=pythonscript for Python'
assert not any(arg.lower().startswith('-executepythonscript=') for arg in args)
command=['bwrap','--ro-bind','/','/','--bind',str(root),str(root),
         '--bind',str(work/'ue-tmp'),'/tmp','--dev-bind','/dev','/dev','--unshare-net','--die-with-parent','--',
         str(engine/'Binaries/Linux/UnrealEditor-Cmd'),str(project),
         '-unattended','-nosplash','-NullRHI','-NoSound','-NoShaderCompile','-ddc=NoZenLocalFallback',
         '-LocalDataCachePath='+str(work/'ddc'),'-UserDir='+str(work/'ue-user')+'/',*args]
with log.open('w') as output:
    result=subprocess.run(command,stdout=output,stderr=subprocess.STDOUT)
raise SystemExit(result.returncode)

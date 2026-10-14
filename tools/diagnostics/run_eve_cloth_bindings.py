"""Bind/reload cloth-transfer intermediates with the existing facial authoring module."""
import argparse
import json
from pathlib import Path
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=('planet','knit','alice'), required=True)
    args = parser.parse_args()
    WORK = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014'/(args.variant+'-ordered')).resolve()
    script = Path(__file__).with_name('bind_eve_cloth_editor.py')
    protected = json.loads((WORK/'binding-plan.json').read_text())['protected']
    protected[str(script)] = sha(script)
    protected[str(Path(__file__))] = sha(Path(__file__))
    for mode in ('create','reload'):
        command_file = WORK/('bind-'+mode+'-command.json')
        if command_file.exists():
            raise FileExistsError(command_file)
        command = json.loads((WORK/'import-command.json').read_text())[:-2]
        position = command.index('--')
        command[position:position] = ['--setenv','CSS_SURFACE_BIND_MODE',mode]
        command += ['-run=pythonscript','-script='+str(script)]
        command_file.write_text(json.dumps(command,indent=2)+'\n')
        with (WORK/('bind-'+mode+'.log')).open('w') as log:
            proc = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
        changed = [p for p,digest in protected.items() if sha(Path(p)) != digest]
        row = dict(returncode=proc.returncode,changed_inputs=changed)
        (WORK/('bind-'+mode+'-exit.json')).write_text(json.dumps(row,indent=2)+'\n')
        print(mode,row,flush=True)
        if proc.returncode or changed or not (WORK/('bindings-'+mode+'.json')).exists():
            raise SystemExit(1)


if __name__ == '__main__':
    main()

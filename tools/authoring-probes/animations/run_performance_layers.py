"""Run layer creation/readback with original outfit graphs mounted read-only."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--mode', choices=('create', 'verify', 'evaluate', 'cook'), required=True)
    parser.add_argument('--label', required=True)
    parser.add_argument('--mesh', help='One audited mesh for an isolated evaluation')
    clips = parser.add_mutually_exclusive_group()
    clips.add_argument('--clip', help='Canonical performance ID from the shared catalog')
    clips.add_argument('--all-moving', action='store_true', help='Evaluate every one-shot moving clip')
    args = parser.parse_args()
    if not re.fullmatch(r'[a-z0-9-]{1,48}', args.label):
        raise ValueError('Use a short log label')
    work = args.work.resolve(strict=True)
    if not work.is_relative_to(ROOT / 'work/cine-animation-port'):
        raise ValueError('Expected an animation work directory')
    command = json.loads((work / 'cook-command.json').read_text())
    command = [item for item in command if item != '-Cook' and not item.startswith(
        ('-run=', '-List=', '-Output=', '-abslog=', '-WindowsRoot='))]
    if command[0] != 'bwrap' or '--ro-bind' not in command:
        raise ValueError('Expected the audited sandboxed authoring command')
    mounts = json.loads((work / 'authoring-source-links.json').read_text())
    content = ROOT.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSS'
    if set(mounts) != {str(content / name) for name in ('UnholyGenessa', 'CWTest', 'CommanderWhite')}:
        raise ValueError('Unexpected source mount destinations')
    bindings = []
    for destination, source in mounts.items():
        target = Path(destination)
        if target.is_symlink() or not target.is_dir() or any(target.iterdir()):
            raise ValueError('Source mount must be an empty real directory: ' + destination)
        if not Path(source).is_dir():
            raise ValueError('Missing original authoring source: ' + source)
        bindings += ['--ro-bind', source, source, '--ro-bind', source, destination]
    position = command.index('--')
    command[position:position] = bindings
    if args.mode == 'cook':
        output = work / (args.label + '-cooked')
        if output.exists():
            raise FileExistsError(output)
        packages = ['/Game/CSS/Locomotion/Adapters/ABP_' + name
                    for name in ('Eve', 'EveBikini', 'EveKnit', 'Genessa', 'CommanderWhite')]
        for package in packages:
            if not (content.parent / (package.removeprefix('/Game/') + '.uasset')).is_file():
                raise ValueError('Missing adapter: ' + package)
        listing = work / (args.label + '-cook.txt')
        with listing.open('x') as file:
            file.write('\n'.join(packages) + '\n')
        command += ['-run=CSSCookAssets', '-Cook', '-WindowsRoot=C:/CSS',
                    '-List=' + str(listing), '-Output=' + str(output)]
    else:
        script = 'evaluate_performance_layers.py' if args.mode == 'evaluate' else 'stage_performance_layers.py'
        command += ['-run=pythonscript', '-script=' + str(Path(__file__).with_name(script))]
    command += ['-abslog=' + str(work / (args.label + '-engine.log'))]
    environment = os.environ.copy()
    environment['CSS_ANIM_WORK'] = str(work)
    environment['CSS_PERFORMANCE_MODE'] = args.mode
    environment['CSS_PERFORMANCE_REPORT'] = args.label + '-result.json'
    if args.mesh:
        if args.mode != 'evaluate':
            raise ValueError('Mesh selection is only available for evaluation')
        environment['CSS_PERFORMANCE_MESH'] = args.mesh
    else:
        environment.pop('CSS_PERFORMANCE_MESH', None)
    if (args.clip or args.all_moving) and args.mode != 'evaluate':
        raise ValueError('Clip selection is only available for evaluation')
    environment.pop('CSS_PERFORMANCE_CLIP', None)
    environment.pop('CSS_PERFORMANCE_ALL_MOVING', None)
    if args.clip:
        environment['CSS_PERFORMANCE_CLIP'] = args.clip
    if args.all_moving:
        environment['CSS_PERFORMANCE_ALL_MOVING'] = '1'
    with (work / (args.label + '-command.json')).open('x') as file:
        json.dump(command, file, indent=2)
    with (work / (args.label + '-console.log')).open('x') as log:
        result = subprocess.run(command, env=environment, stdout=log, stderr=subprocess.STDOUT)
    with (work / (args.label + '-exit.json')).open('x') as file:
        json.dump(dict(exit_code=result.returncode), file)
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()

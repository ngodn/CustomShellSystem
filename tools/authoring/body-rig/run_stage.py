#!/usr/bin/env python3
"""Run one body-rig editor script in a private, read-only-everywhere-else stage.

    run_stage.py STAGE SCRIPT [editor args...] [--log NAME]
    run_stage.py STAGE --commandlet NAME [editor args...] [--log NAME]

STAGE gets an editor project whose Content links the CSSAuthoring content
read-only, except Content/CSSAuthoring/TransientProbes and Content/CSS/
TransientProbes, real directories where new rigs and AnimBPs are saved
(existing probe assets are linked in). The editor runs under
bwrap with / bound read-only and only STAGE writable, so nothing outside it can
change. command.json records the exact argv; <log>.engine.log the editor log.

--module adds the CSSAuthoring editor module, built from STAGE/editor/Source
(see the combined AnimBP step), instead of a content-only project.
--writable CSS/<outfit> (repeatable) makes one more Content folder a real
directory in the stage, for steps that must save beside an outfit's rigs.
--commandlet runs that editor commandlet (for example CSSImportMesh) instead of
a Python script.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]
ENGINE = ROOT/'CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed'
CONTENT = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
PLUGINS = ['PythonScriptPlugin', 'ControlRig', 'ChaosClothAsset', 'EditorScriptingUtilities']
LIBRARIES = ['Engine/Binaries/Linux', 'Engine/Plugins/Animation/IKRig/Binaries/Linux',
             'Engine/Plugins/Animation/ControlRig/Binaries/Linux', 'Engine/Plugins/Runtime/RigVM/Binaries/Linux',
             'Engine/Plugins/ChaosClothAsset/Binaries/Linux', 'Engine/Plugins/ChaosCloth/Binaries/Linux']


def link_tree(source, target, writable):
    """Mirror source into target with symlinks; writable paths become real directories.

    writable holds tuples of path parts below source. A real directory keeps
    links to everything that already exists in the source.
    """
    target.mkdir(parents=True, exist_ok=True)
    for entry in source.iterdir():
        destination = target/entry.name
        below = {path[1:] for path in writable if path[0] == entry.name}
        if below:
            if destination.is_symlink():
                destination.unlink()
            if () in below:
                destination.mkdir(exist_ok=True)
                for item in entry.iterdir():
                    if not (destination/item.name).exists():
                        (destination/item.name).symlink_to(item)
            else:
                link_tree(entry, destination, below)
        elif not destination.exists() and not destination.is_symlink():
            destination.symlink_to(entry)


def prepare(stage, module, writable=()):
    editor = stage/'editor'
    for name in ('tmp', 'cache', 'ddc', 'user', 'config-home', 'editor'):
        (stage/name).mkdir(parents=True, exist_ok=True)
    folders = {('CSSAuthoring', 'TransientProbes'), ('CSS', 'TransientProbes')}
    for folder in writable:
        parts = tuple(Path(folder).parts)
        if len(parts) != 2 or parts[0] not in ('CSS', 'CSSAuthoring') or '..' in parts:
            raise ValueError('Writable folders are CSS/<outfit> or CSSAuthoring/<outfit>')
        folders.add(parts)
    link_tree(CONTENT, editor/'Content', folders)
    project = dict(FileVersion=3, Plugins=[dict(Name=p, Enabled=True) for p in PLUGINS]
                   + [dict(Name='AndroidFileServer', Enabled=False)])
    if module:
        project['Modules'] = [dict(Name='CSSAuthoring', Type='Editor', LoadingPhase='Default')]
    (editor/'BodyRig.uproject').write_text(json.dumps(project, indent=2)+'\n')
    return editor/'BodyRig.uproject'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('stage', type=Path)
    parser.add_argument('script', type=Path, nargs='?')
    parser.add_argument('--commandlet')
    parser.add_argument('--log', required=True)
    parser.add_argument('--module', action='store_true')
    parser.add_argument('--writable', action='append', default=[])
    args, extra = parser.parse_known_args()
    if bool(args.script) == bool(args.commandlet):
        parser.error('Give either a script or --commandlet')
    stage = args.stage.resolve()
    project = prepare(stage, args.module, args.writable)
    argv = ['nice', '-n', '19', 'bwrap', '--unshare-net', '--ro-bind', '/', '/',
            '--bind', str(stage), str(stage), '--bind', str(stage/'tmp'), '/tmp',
            '--dev-bind', '/dev', '/dev', '--proc', '/proc', '--die-with-parent',
            '--setenv', 'CSS_BODY_RIG_STAGE', str(stage),
            '--setenv', 'PYTHONDONTWRITEBYTECODE', '1',
            '--setenv', 'XDG_CACHE_HOME', str(stage/'cache'),
            '--setenv', 'XDG_CONFIG_HOME', str(stage/'config-home'),
            '--setenv', 'LD_LIBRARY_PATH', ':'.join(str(ENGINE/p) for p in LIBRARIES),
            '--', str(ENGINE/'Engine/Binaries/Linux/UnrealEditor-Cmd'), str(project),
            *(['-run=' + args.commandlet] if args.commandlet else
              ['-run=pythonscript', '-script=' + str(args.script.resolve())]), *extra,
            '-unattended', '-nosplash', '-NoSound', '-NullRHI', '-NoShaderCompile',
            '-ddc=NoZenLocalFallback', '-LocalDataCachePath=' + str(stage/'ddc'),
            '-UserDir=' + str(stage/'user') + '/', '-abslog=' + str(stage/(args.log + '.engine.log')),
            '-NoCrashReports', '-NoCrashDialog']
    (stage/(args.log + '.command.json')).write_text(json.dumps(argv, indent=2)+'\n')
    with open(stage/(args.log + '.log'), 'w') as log:
        code = subprocess.run(argv, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL).returncode
    (stage/(args.log + '.exit.json')).write_text(json.dumps(dict(returncode=code))+'\n')
    print(args.log, 'exit', code)
    raise SystemExit(code)


if __name__ == '__main__':
    main()

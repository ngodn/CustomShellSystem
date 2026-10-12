"""Isolate constraints on the retained continuous Slipstream simulation surface."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
EVE = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
BASE = EVE / 'work/slipstream-cloth-native18'
OUT = ROOT / 'work/three-outfit-rig-physics/slipstream-continuous-isolation1'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUT)
    parser.add_argument('--reverse', action='store_true')
    args = parser.parse_args()
    OUT = args.output.resolve()
    if OUT.parent != (ROOT / 'work/three-outfit-rig-physics').resolve() or not OUT.name.startswith('slipstream-continuous-isolation'):
        raise ValueError('Use a private Slipstream isolation directory')
    OUT.mkdir(exist_ok=False)
    previous = json.loads((BASE / 'continuous-motion.exit.json').read_text())
    if previous['returncode'] != 0:
        raise ValueError('Retained baseline did not complete')
    for name, digest in previous['module'].items():
        if sha(BASE / 'editor' / name) != digest:
            raise ValueError('Diagnostic module changed: ' + name)
    content = BASE / 'editor/Content'
    protected = {str(path): sha(path) for path in content.rglob('*.uasset')}
    flags = dict(baseline='', no_body=' -NoBodyCollision', no_backstop=' -NoBackstop',
                 no_self=' -NoSelfCollision', no_drive=' -NoAnimDrive')
    if args.reverse:
        flags = dict(reversed(list(flags.items())))
    common = ('-Mesh=/Game/CSS/eins0fx/EveSB/Slipstream/Meshes/SK_SlipstreamGarments '
              '-ClothIndex=0 -Frames=145 -FPS=60 -Secondary '
              '-Animation=/Game/CSS/eins0fx/Shared/Locomotion/EveLegacy/AN_S1_Sprint -Clip=Sprint')
    cases = [common + flag + ' -Report=' + str(OUT / (name + '.json'))
             for name, flag in flags.items()]
    batch = OUT / 'batch.json'
    batch.write_text(json.dumps(cases, indent=2) + '\n')
    command = previous['command'][:]
    boundary = command.index('--')
    command[boundary:boundary] = ['--bind', str(OUT), str(OUT),
                                '--ro-bind', str(content), str(content)]
    command = ['-MotionBatch=' + str(batch) if arg.startswith('-MotionBatch=') else arg
               for arg in command]
    command = ['nice', '-n', '19'] + command + ['-corelimit=2', '-NoCrashReports']
    receipt = dict(command=command, protected=protected, module=previous['module'],
                   engine=previous['engine'], recipe_sha256=sha(Path(__file__)),
                   case_order=list(flags),
                   scope='Private secondary sprint; one constraint category removed per case, no saved edits.')
    (OUT / 'command.json').write_text(json.dumps(receipt, indent=2) + '\n')
    with (OUT / 'command.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    receipt['returncode'] = result.returncode
    receipt['changed_assets'] = [name for name, digest in protected.items() if sha(Path(name)) != digest]
    (OUT / 'exit.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if receipt['changed_assets']:
        raise RuntimeError('Source packages changed')
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()

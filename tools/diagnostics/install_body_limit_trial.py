"""Prepare rollback, then install the verified motion assets while the game is closed."""
import argparse
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME, atomic, copy_verified, processes, sha


def read(path):
    return json.loads(path.read_text())


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--trial', type=Path, required=True)
parser.add_argument('--baseline', type=Path, required=True)
parser.add_argument('--backup', type=Path, required=True)
parser.add_argument('--install', action='store_true')
args = parser.parse_args()
trial, baseline = args.trial.resolve(strict=True), args.baseline.resolve(strict=True)
backup = args.backup.resolve()
external = Path('/run/media/eins0fx/theark')
if not os.path.ismount(external) or not backup.is_relative_to(external/'CSS'):
    raise ValueError('Expected the mounted external CSS backup drive')
verification = read(trial/'verified.json')
assembly, old = read(trial/'assembly.json'), read(baseline/'assembly.json')
if not verification['passed'] or sha(trial/'assembly.json') != verification['assembly_sha256']:
    raise ValueError('Package verification is stale')
eve, shared = 'CSS_EveStellarBlade_eins0fx_P', 'CSS_SharedAssets_P'
names = [eve+'/'+eve+suffix for suffix in ('.pak', '.utoc', '.ucas')]+[shared+'/'+shared+'.pak']
mods = GAME/'Content/Paks/~mods'
plan_path = trial/'installation-plan.json'
if not plan_path.exists():
    backup.mkdir(parents=True, exist_ok=False)
    rows = []
    for name in names:
        current, accepted = mods/name, baseline/'pack'/name
        expected = old['files'][name]
        if sha(current) != expected or sha(accepted) != expected:
            raise ValueError('Installed package differs from the accepted baseline: '+name)
        saved = backup/name
        saved.parent.mkdir(parents=True, exist_ok=True)
        # The accepted package is already on the external drive. Preserve its bytes without another bulk copy.
        os.link(accepted, saved)
        if sha(saved) != expected:
            raise ValueError('Rollback hash differs')
        source = trial/'pack'/name
        if sha(source) != assembly['files'][name]:
            raise ValueError('Candidate file changed')
        rows.append(dict(source=str(source), target=str(current), backup=str(saved),
                         before=expected, after=assembly['files'][name]))
    plan = dict(files=rows, backup=str(backup),
                scope='Eve body limits and shared Eve adapter fingerprints; no core DLL or settings change',
                verification_sha256=sha(trial/'verified.json'))
    atomic(backup/'receipt.json', plan)
    atomic(plan_path, plan)
plan = read(plan_path)
if plan['backup'] != str(backup) or plan['verification_sha256'] != sha(trial/'verified.json'):
    raise ValueError('Installation plan differs')
if not args.install:
    print('Rollback and four-file installation plan prepared; game files unchanged')
    raise SystemExit(0)
if processes():
    raise RuntimeError('Close the game before replacing mounted assets')
if (trial/'deployment.json').exists():
    raise FileExistsError('This candidate has an installation record already')
for row in plan['files']:
    for key, expected in (('target', row['before']), ('backup', row['before']), ('source', row['after'])):
        if sha(Path(row[key])) != expected:
            raise ValueError('Installation input changed: '+row[key])
staged, replaced = [], []
try:
    for row in plan['files']:
        target = Path(row['target'])
        temporary = target.with_name(target.name+'.body-limits-stage')
        if temporary.exists():
            raise FileExistsError(temporary)
        staged.append((row, temporary))
        copy_verified(Path(row['source']), temporary)
    if processes():
        raise RuntimeError('Game reopened before asset replacement')
    for row, temporary in staged:
        replaced.append(row)
        os.replace(temporary, row['target'])
    for row in plan['files']:
        if sha(Path(row['target'])) != row['after']:
            raise ValueError('Installed hash differs')
    atomic(trial/'deployment.json', dict(installed=True, files=plan['files'], backup=str(backup),
                                       activation='Next game launch', gameplay_verified=False))
except BaseException:
    for row in reversed(replaced):
        copy_verified(Path(row['backup']), Path(row['target']))
    raise
finally:
    for _, temporary in staged:
        if temporary.exists():
            temporary.unlink()
print('Four verified asset files installed; open the game to check motion')

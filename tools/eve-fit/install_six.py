"""Replace the six backed-up Eve packages with the final trio. Python 3.14."""
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import processes
from css_convert import digest
from css_package import verify
from convert_beaute import DEFAULT_GAME


def matches(directory, files):
    if {p.name for p in directory.iterdir()} != set(files):
        raise ValueError(f'Unexpected contents: {directory}')
    for name, expected in files.items():
        if digest(directory / name) != expected:
            raise ValueError(f'File changed: {directory / name}')


def main():
    if processes():
        raise RuntimeError('Game is running; replacement is prepared for the next normal restart.')
    stem = 'CSS_EveStellarBlade_eins0fx_P'
    work = ROOT / 'work/eve26'
    candidate = work / 'six-candidate1' / stem
    manifest = verify(candidate)
    if len(manifest['catalog']['outfits'][0]['variants']) != 6:
        raise ValueError('Expected six release variants')
    proof = json.loads((work / 'six-candidate1/verification.json').read_text())
    matches(candidate, proof['files'])
    backup = ROOT / 'backups/eve-six1'
    records = json.loads((backup / 'receipt.json').read_text())['packages']
    if len(records) != 6:
        raise ValueError('Expected old Eve and five split backups')
    mods = DEFAULT_GAME / 'Content/Paks/~mods'
    for row in records:
        installed, saved = Path(row['installed']), Path(row['backup'])
        if installed.parent != mods or saved.parent != backup:
            raise ValueError('Backup receipt points outside expected directories')
        matches(installed, row['files'])
        matches(saved, row['files'])
    transaction = DEFAULT_GAME / 'Content/eve-swap1'
    transaction.mkdir(exist_ok=False)
    stage = transaction / 'new' / stem
    shutil.copytree(candidate, stage)
    matches(stage, proof['files'])
    verify(stage)
    old = transaction / 'old'
    old.mkdir()
    retired = []
    destination = mods / stem
    published = False
    try:
        if processes():
            raise RuntimeError('Game started during staging; installed packages left untouched')
        for row in records:
            source = Path(row['installed'])
            matches(source, row['files'])
            moved = old / source.name
            source.rename(moved)
            retired.append((source, moved))
        stage.rename(destination)
        published = True
        matches(destination, proof['files'])
        verify(destination)
    except Exception:
        if published:
            destination.rename(stage)
        for source, moved in reversed(retired):
            moved.rename(source)
        raise
    receipt = dict(destination=str(destination), files=proof['files'],
                   retired=[str(source) for source, _ in retired],
                   backup=str(backup), activation='Next normal launch; no game input sent')
    (work / 'six-candidate1/install.json').write_text(json.dumps(receipt, indent=2)+'\n')
    # Every retired file has a verified workspace backup; no other package enters this directory.
    shutil.rmtree(transaction)
    print(destination)


if __name__ == '__main__':
    main()

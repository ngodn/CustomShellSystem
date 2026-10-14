"""Recover missing original material inputs from the hash-pinned Eve rollback.

Python 3.14. Keep unique headers and link byte-identical exports to retained
accepted payloads after verifying the complete extraction.
"""
import json
from pathlib import Path
import shutil
import subprocess
import sys

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK

WORK = ROOT/'work/three-outfit-rig-physics'
OUT = ROOT/'build/eve-material-source-recovery1'
ORIGINAL = WORK/'eve-material-namespace1/original'
RETOC = ROOT/'build/retoc-css-target/release/retoc'


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    require(not OUT.exists() and not ORIGINAL.exists() and not ORIGINAL.is_symlink(), 'Recovery or original path already exists')
    require(OUT.parent.resolve() == OUT.parent and shutil.disk_usage(OUT.parent).free > 3*1024**3,
            'Need root staging and 3 GiB free')
    inputs = {}

    def pin(path, digest=None):
        actual = sha(path)
        require(digest is None or digest == actual, 'Changed recovery input: '+str(path))
        inputs[str(path)] = actual

    material_path = WORK/'eve-material-namespace1/verification.json'
    texture_path = WORK/'eve-texture-namespace2/verification.json'
    plan_path = ROOT/'build/eve-surface-package1/plan.json'
    prelist = ROOT/'docs/development/cleanup-2026-10-14-eve-material-source-recovery.md'
    for path in (material_path, texture_path, plan_path, prelist, RETOC, Path(__file__)):
        pin(path)
    material, texture, candidate = read(material_path), read(texture_path), read(plan_path)
    require(material['passed'] and texture['passed'], 'Original preservation receipts did not pass')
    source_files = {**material['source_files'], **{p:h for p,h in texture['source_files'].items() if Path(p).is_relative_to(ORIGINAL)}}
    expected = {str(Path(p).relative_to(ORIGINAL)): h for p,h in source_files.items()}
    require(len(expected) == 745, 'Expected the recorded 272 original material/texture packages')
    backup = GAME/'Content/CSS-namespace-trial1/backup/CSS_EveStellarBlade_eins0fx_P'
    for suffix, row in material['source_containers'].items():
        pin(backup/('CSS_EveStellarBlade_eins0fx_P'+suffix), row['sha256'])
    OUT.mkdir()
    df = ['df', '-h', '/', '/run/media/eins0fx/theark']
    before = subprocess.check_output(df, text=True)
    converter = Converter(RETOC, DEFAULT_REPAK, OUT)
    converter.base_containers(GAME, OUT/'containers')
    for suffix in ('.utoc', '.ucas'):
        path = backup/('CSS_EveStellarBlade_eins0fx_P'+suffix)
        (OUT/'containers'/path.name).symlink_to(path)
    command = [str(RETOC), 'to-legacy', str(OUT/'containers'), str(OUT/'original'),
               '--version', 'UE5_6', '--no-parallel', '--no-shaders', '--no-script-objects']
    for relative in sorted(p for p in expected if p.endswith('.uasset')):
        command += ['-f', '/'+relative]
    write(OUT/'plan.json', dict(inputs=inputs, expected=expected, before=before, source_backup=str(backup)))
    write(OUT/'command.json', command)
    with (OUT/'extract.log').open('w') as log:
        proc = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    write(OUT/'exit.json', dict(returncode=proc.returncode))
    proc.check_returncode()
    actual = {str(p.relative_to(OUT/'original')) for p in (OUT/'original').rglob('*') if p.is_file()}
    require(actual == set(expected), 'Extraction file set differs')
    for relative, digest in expected.items():
        require(sha(OUT/'original'/relative) == digest, 'Original extraction hash differs: '+relative)
    retained_by_hash = {}
    for row in candidate['payloads'].values():
        retained_by_hash.setdefault(row['sha256'], Path(row['path']).resolve(strict=True))
    replacements = []
    for relative, digest in expected.items():
        retained = retained_by_hash.get(digest)
        if retained is None:
            continue
        pin(retained, digest)
        target = OUT/'original'/relative
        require(not target.is_symlink(), 'Unexpected extraction symlink')
        replacements.append(dict(path=str(target), retained=str(retained), sha256=digest, bytes=target.stat().st_size))
    require(all(sha(Path(p)) == h for p,h in inputs.items()), 'Input changed during extraction')
    cleanup = dict(replacements=replacements, prelist_sha256=sha(prelist), inputs=inputs,
                   before=subprocess.check_output(df, text=True))
    write(OUT/'cleanup-plan.json', cleanup)
    for row in replacements:
        target = Path(row['path'])
        require(sha(target) == row['sha256'] and sha(Path(row['retained'])) == row['sha256'], 'Duplicate changed before replacement')
        target.unlink()
        target.symlink_to(row['retained'])
    for relative, digest in expected.items():
        require(sha(OUT/'original'/relative) == digest, 'Recovered source changed')
    require(not ORIGINAL.exists() and not ORIGINAL.is_symlink(), 'Original path appeared during recovery')
    ORIGINAL.symlink_to(OUT/'original', target_is_directory=True)
    write(OUT/'verified.json', dict(passed=True, original_link=str(ORIGINAL), physical_path=str(OUT/'original'),
        files=len(expected), linked_duplicates=len(replacements), unique_files=len(expected)-len(replacements),
        duplicate_logical_bytes=sum(r['bytes'] for r in replacements), inputs=inputs,
        original_hashes=expected, before=before, cleanup_before=cleanup['before'],
        after=subprocess.check_output(df, text=True), installed_files_changed=False))
    print('Recovered', len(expected), 'original inputs;', len(replacements), 'byte-identical payloads linked')


if __name__ == '__main__':
    main()

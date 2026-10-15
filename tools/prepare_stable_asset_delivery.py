"""Stage accepted outfit/shared containers under dist without installing them.

Python 3.14. Only release versions change; the accepted cooked data stays exact.
These are local delivery candidates, not a release tag or publication.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

from css import GAME, ROOT, atomic
from css_convert import DEFAULT_REPAK
from css_package import release_zip, verify

AUTHOR = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections'
SPECS = {
    'genessa': ('CSS_UnholyGenessa_eins0fx_P', '0.3.0', 'unholygenessa-v0.3.0',
        AUTHOR / 'CSS_UnholyGenessa_eins0fx_P/work/hair-solver-pack3', 'verification.json', ''),
    'commanderwhite': ('CSS_CommanderWhite_eins0fx_P', '0.1.0', 'commanderwhite-v0.1.0',
        AUTHOR / 'CSS_CommanderWhite_eins0fx_P/work/walk-final-paths1/assembly', 'verification.json', ''),
    'shared': ('CSS_SharedAssets_P', '1.0.0', 'v1.0.0',
        ROOT / 'build/shared-boss-appearances1', 'verified.json', 'pack'),
}


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', choices=SPECS)
    args = parser.parse_args()
    name, version, folder, base, receipt_name, prefix = SPECS[args.package]
    source = base / prefix / name
    proof_path = base / receipt_name
    proof = json.loads(proof_path.read_text())
    assert proof['passed']
    files = {name + suffix: proof['files'][name + suffix] for suffix in ('.pak', '.utoc', '.ucas')}
    for filename, expected in files.items():
        assert sha(source / filename) == expected, filename
        assert sha(GAME / 'Content/Paks/~mods' / name / filename) == expected, 'Installed source differs'
    output = ROOT / 'dist' / folder
    output.mkdir(parents=True, exist_ok=True)
    archive = output / (name + '-v' + version + '.zip')
    if archive.exists():
        raise FileExistsError(archive)
    work = ROOT / 'build/stable-asset-delivery' / args.package
    work.mkdir(parents=True, exist_ok=False)
    stage, metadata = work / name, work / 'metadata'
    stage.mkdir()
    run(DEFAULT_REPAK, 'unpack', source / (name + '.pak'), '--output', metadata)
    manifest_path, = metadata.rglob('manifest.json')
    before = json.loads(manifest_path.read_text())
    resources = {str(p.relative_to(metadata)): sha(p) for p in metadata.rglob('*')
                 if p.is_file() and p != manifest_path}
    after = copy.deepcopy(before)
    after['version'] = version
    if 'animation_library' in after['catalog']:
        after['catalog']['animation_library']['version'] = version
    for outfit in after['catalog'].get('outfits', []):
        outfit['version'] = version
    manifest_path.write_text(json.dumps(after, separators=(',', ':')) + '\n')
    atomic(work / 'manifest-before.json', before)
    atomic(work / 'manifest-after.json', after)
    for suffix in ('.utoc', '.ucas'):
        run('cp', '--reflink=auto', source / (name + suffix), stage / (name + suffix))
    run(DEFAULT_REPAK, 'pack', metadata, stage / (name + '.pak'), '--version', 'V8B')
    unpacked = work / 'readback-metadata'
    run(DEFAULT_REPAK, 'unpack', stage / (name + '.pak'), '--output', unpacked)
    actual_path, = unpacked.rglob('manifest.json')
    assert json.loads(actual_path.read_text()) == after
    assert resources == {str(p.relative_to(unpacked)): sha(p) for p in unpacked.rglob('*')
                         if p.is_file() and p != actual_path}
    for suffix in ('.utoc', '.ucas'):
        p = stage / (name + suffix)
        assert sha(p) == files[p.name]
        assert after['containers'][suffix] == {'file': p.name, 'bytes': p.stat().st_size, 'sha256': files[p.name]}
    payloads = {p.name: sha(p) for p in stage.iterdir()}
    if args.package != 'shared':
        assert verify(stage) == after
        release_zip(stage, archive)
    else:
        temporary = output / (archive.name + '.tmp')
        with zipfile.ZipFile(temporary, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for p in sorted(stage.iterdir()):
                z.write(p, name + '/' + p.name)
        with zipfile.ZipFile(temporary) as z:
            assert set(z.namelist()) == {name + '/' + filename for filename in payloads}
            assert len(z.namelist()) == 3 and z.testzip() is None
            for filename, expected in payloads.items():
                with z.open(name + '/' + filename) as stream:
                    assert hashlib.file_digest(stream, 'sha256').hexdigest() == expected
        temporary.rename(archive)
    digest = sha(archive)
    archive.with_suffix('.zip.sha256').write_text(digest + '  ' + archive.name + '\n')
    report = dict(passed=True, version=version, source_candidate=str(base), source_receipt_sha256=sha(proof_path),
        source_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        recipe_sha256=sha(Path(__file__)), source_files=files, payloads=payloads,
        archive=archive.name, sha256=digest, bytes=archive.stat().st_size,
        metadata_changes='Release version only', metadata_resources_preserved=len(resources),
        cooked_data_matches_installed=True, zip_roundtrip_verified=True, game_files_changed=False,
        variants=[v['id'] for o in after['catalog'].get('outfits', []) for v in o['variants']],
        published=False, coordinated_release_complete=False)
    report_name = 'shared-verification.json' if args.package == 'shared' else 'verification.json'
    atomic(output / report_name, report)
    atomic(work / 'verification.json', report)
    status_name = 'SHARED-RELEASE-STATUS.json' if args.package == 'shared' else 'RELEASE-STATUS.json'
    atomic(output / status_name, dict(status='current_candidate_packaged', version=version,
        archive=archive.name, sha256=digest, published=False, coordinated_release_complete=False))
    readme = 'SHARED-README.md' if args.package == 'shared' else 'README.md'
    (output / readme).write_text(f'# {after["name"]} v{version}\n\n'
        'Close the game. Extract the package folder into MortalShell2/Content/Paks/~mods,\n'
        'replacing its previous version. Keep all three files together. Preserve CSS settings\n'
        'and profiles. Use CSS v1.0.0 and the matching combined CSS Shared Assets download.\n\n'
        + ('Remove superseded CSS_AstralSharedAssets_P and CSS_LocomotionSharedAssets_P\n'
           'packages when installing this combined package. Keep only one shared provider.\n\n' if args.package == 'shared' else '')
        + 'This local delivery contains the accepted cooked data with updated release metadata.\n'
        'Archive integrity and payload hashes are verified. Coordinated release completion\n'
        'is still pending; this archive has not been published. See the adjacent verification\n'
        'and release-status files. Shared Assets changes are in the CSS changelog.\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()

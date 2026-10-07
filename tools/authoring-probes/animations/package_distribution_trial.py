"""Archive a verified shared trial without changing assets or installing it. Python 3.14."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time
import zipfile

from install_shared_animation_trial import validate_shared_package
from update_performance_trial import NAME, ROOT, sha

DOCS = {
    'README.txt': 'packaging/locomotion-trial-README.txt',
    'CREDITS.txt': 'packaging/locomotion-trial-CREDITS.txt',
    'description.bbcode.txt': 'docs/nexus/locomotion-trial.bbcode.txt',
}
ASSETS = {NAME + suffix for suffix in ('.pak', '.utoc', '.ucas')}
MEMBERS = ASSETS | set(DOCS) | {'candidate.json'}
ARCHIVE = 'CSS-LocomotionSharedAssets-v0.1.0-dev-trial.zip'


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def runtime_identity(runtime, candidate_path=None):
    hashes = {'css_core': runtime['core_sha256'], 'cine': runtime['cine_sha256']}
    status = 'installed_runtime'
    evidence = {}
    if candidate_path is not None:
        candidate_path = candidate_path.resolve(strict=True)
        candidate = json.loads(candidate_path.read_text())
        core = candidate_path.parent / 'core.dll'
        if (candidate.get('built') is not True or candidate.get('installed') is not False
                or candidate.get('live_verified') is not False
                or candidate.get('tests', {}).get('exit_code') != 0
                or candidate.get('core_sha256') != sha(core)):
            raise ValueError('Invalid or changed staged core')
        previous = Path(candidate['previous_receipt']).resolve(strict=True)
        if json.loads(previous.read_text()) != runtime:
            raise ValueError('Staged core targets another installed runtime')
        hashes['css_core'] = candidate['core_sha256']
        status = 'staged_core_live_acceptance_pending'
        evidence = {str(p): sha(p) for p in (candidate_path, core, previous)}
    if any(not isinstance(h, str) or len(h) != 64 or
           any(c not in '0123456789abcdef' for c in h) for h in hashes.values()):
        raise ValueError('Invalid runtime hashes')
    return hashes, status, evidence


def verify(archive):
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        if len(names) != len(MEMBERS) or set(names) != {NAME + '/' + n for n in MEMBERS}:
            raise ValueError('Missing, duplicate or unexpected archive members')
        manifest = json.loads(z.read(NAME + '/candidate.json'))
        if (manifest.get('kind') != 'private_trial' or manifest.get('version') != '0.1.0-dev'
                or manifest.get('container') != NAME or manifest.get('entries') != 63
                or manifest.get('exports') != 99 or manifest.get('accepted_for_release') is not False
                or set(manifest.get('files', {})) != MEMBERS - {'candidate.json'}):
            raise ValueError('Invalid private-trial manifest')
        if manifest.get('runtime_status') not in {'installed_runtime', 'staged_core_live_acceptance_pending'}:
            raise ValueError('Invalid runtime status')
        hashes = manifest.get('runtime_hashes', {})
        if set(hashes) != {'css_core', 'cine'}:
            raise ValueError('Invalid runtime hashes')
        runtime_identity({'core_sha256': hashes['css_core'], 'cine_sha256': hashes['cine']})
        for name, checksum in manifest['files'].items():
            with z.open(NAME + '/' + name) as f:
                if hashlib.file_digest(f, 'sha256').hexdigest() != checksum:
                    raise ValueError('Archive checksum mismatch: ' + name)
        return manifest


def write_archive(files, destination, timestamp):
    if set(files) != MEMBERS:
        raise ValueError('Unexpected payload set')
    with zipfile.ZipFile(destination, 'x') as z:
        for name, path in sorted(files.items()):
            info = zipfile.ZipInfo(NAME + '/' + name, timestamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.compress_level = 6
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            with path.open('rb') as source, z.open(info, 'w', force_zip64=True) as target:
                shutil.copyfileobj(source, target, 1024 * 1024)
    return verify(destination)


def build(args):
    pack = args.pack.resolve(strict=True)
    output = args.output.absolute()
    if output.exists():
        raise FileExistsError('Use a fresh output directory')
    verified, evidence = validate_shared_package(pack, {}, args.baseline)
    metadata = pack / 'metadata/MortalShell2/Content/CSS/Packages/css.locomotion/manifest.json'
    catalog = json.loads(metadata.read_text())
    entries = catalog['catalog']['animation_library']['entries']
    if (catalog.get('kind') != 'animation_library' or catalog.get('id') != 'css.locomotion'
            or catalog.get('version') != '0.1.0-dev' or len(entries) != 63
            or len({entry['id'] for entry in entries}) != 63 or verified['exports'] != 99):
        raise ValueError('Unexpected candidate catalog')
    runtime = json.loads(args.runtime_receipt.read_text())
    if runtime.get('installed') is not True or runtime.get('assets') != verified['files']:
        raise ValueError('Runtime receipt describes another asset set')
    runtime_hashes, runtime_status, runtime_evidence = runtime_identity(runtime, args.core_candidate)
    evidence.update(runtime_evidence)
    revision = git('rev-parse', '--verify', args.notes_ref + '^{commit}').decode().strip()
    docs = {name: git('show', revision + ':' + path) for name, path in DOCS.items()}
    stamp = time.gmtime(max(315532800, int(git('show', '-s', '--format=%ct', revision))))[:6]
    evidence[str(metadata)] = sha(metadata)
    evidence[str(args.runtime_receipt.resolve())] = sha(args.runtime_receipt)
    output.mkdir(parents=True, exist_ok=False)
    files = {name: pack / NAME / name for name in ASSETS}
    for name, data in docs.items():
        files[name] = output / name
        files[name].write_bytes(data)
    manifest = dict(kind='private_trial', version='0.1.0-dev', container=NAME,
        entries=63, exports=99, notes_commit=revision, accepted_for_release=False,
        floor_contact='deferred_by_user',
        runtime_hashes=runtime_hashes, runtime_status=runtime_status,
        files={name: sha(path) for name, path in sorted(files.items())})
    manifest_path = output / 'candidate.json'
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    files['candidate.json'] = manifest_path
    archive = output / ARCHIVE
    actual = write_archive(files, archive, stamp)
    if actual['files'] != manifest['files'] or any(actual['files'][n] != verified['files'][n] for n in ASSETS):
        raise ValueError('Archived inputs differ from verified candidate')
    result = dict(passed=True, archive=ARCHIVE, sha256=sha(archive), bytes=archive.stat().st_size,
                  accepted_for_release=False, evidence=evidence)
    (output / 'verification.json').write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    (output / 'SHA256SUMS.txt').write_text(''.join(
        f'{sha(path)}  {path.name}\n' for path in sorted(output.iterdir()) if path.is_file()))
    print(archive)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    make = sub.add_parser('build')
    for key in ('pack', 'baseline', 'runtime-receipt', 'output'):
        make.add_argument('--' + key, type=Path, required=True)
    make.add_argument('--notes-ref', required=True, help='Commit containing the exact notes to ship')
    make.add_argument('--core-candidate', type=Path,
                      help='Verified staged core candidate.json, clearly marked as not yet live-accepted')
    check = sub.add_parser('verify')
    check.add_argument('archive', type=Path)
    args = parser.parse_args()
    if args.action == 'build':
        build(args)
    else:
        print(json.dumps(verify(args.archive), indent=2))

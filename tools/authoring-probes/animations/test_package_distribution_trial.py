import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import warnings
import zipfile

import package_distribution_trial as package


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.files = {}
        for name in package.MEMBERS - {'candidate.json'}:
            path = self.root / name
            path.write_bytes((name + '\n').encode() * 7)
            self.files[name] = path
        self.manifest = dict(kind='private_trial', version='0.1.0-dev', container=package.NAME,
            entries=63, exports=99, accepted_for_release=False,
            runtime_status='installed_runtime', runtime_hashes={'css_core': 'a'*64, 'cine': 'b'*64},
            files={name: package.sha(path) for name, path in self.files.items()})
        self.files['candidate.json'] = self.root / 'candidate.json'
        self.save_manifest()
        self.timestamp = (2026, 10, 7, 0, 0, 0)

    def save_manifest(self):
        self.files['candidate.json'].write_text(json.dumps(self.manifest, sort_keys=True))

    def test_identical_inputs_produce_identical_archive_bytes(self):
        first, second = self.root / 'first.zip', self.root / 'second.zip'
        for target in (first, second):
            package.write_archive(self.files, target, self.timestamp)
        self.assertEqual(package.sha(first), package.sha(second))

    def test_container_changed_since_validation_is_rejected(self):
        self.files[package.NAME + '.ucas'].write_bytes(b'changed container')
        with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
            package.write_archive(self.files, self.root / 'changed.zip', self.timestamp)

    def test_duplicate_or_unexpected_archive_member_is_rejected(self):
        for member in (package.NAME + '/README.txt', '../unwanted.dll'):
            with self.subTest(member=member):
                archive = self.root / (hashlib.sha256(member.encode()).hexdigest() + '.zip')
                package.write_archive(self.files, archive, self.timestamp)
                with warnings.catch_warnings():
                    warnings.simplefilter('ignore', UserWarning)
                    with zipfile.ZipFile(archive, 'a') as z:
                        z.writestr(member, b'extra')
                with self.assertRaisesRegex(ValueError, 'archive members'):
                    package.verify(archive)

    def test_trial_cannot_be_marked_release_accepted(self):
        self.manifest['accepted_for_release'] = True
        self.save_manifest()
        with self.assertRaisesRegex(ValueError, 'Invalid private-trial'):
            package.write_archive(self.files, self.root / 'false-release.zip', self.timestamp)

    def test_existing_archive_is_not_overwritten(self):
        archive = self.root / 'existing.zip'
        archive.write_bytes(b'keep this')
        with self.assertRaises(FileExistsError):
            package.write_archive(self.files, archive, self.timestamp)
        self.assertEqual(archive.read_bytes(), b'keep this')

    def staged_core(self):
        runtime = {'core_sha256': 'a'*64, 'cine_sha256': 'b'*64, 'installed': True}
        previous = self.root / 'previous.json'
        previous.write_text(json.dumps(runtime))
        core = self.root / 'core.dll'
        core.write_bytes(b'new compiled core')
        candidate = self.root / 'staged.json'
        candidate.write_text(json.dumps(dict(built=True, installed=False, live_verified=False,
            tests={'exit_code': 0}, core_sha256=package.sha(core), previous_receipt=str(previous))))
        return runtime, core, candidate

    def test_staged_core_is_distinguished_from_installed_runtime(self):
        runtime, core, candidate = self.staged_core()
        hashes, status, evidence = package.runtime_identity(runtime, candidate)
        self.assertEqual(hashes, {'css_core': package.sha(core), 'cine': runtime['cine_sha256']})
        self.assertEqual(status, 'staged_core_live_acceptance_pending')
        self.assertEqual(evidence[str(core)], package.sha(core))
        self.assertEqual(runtime['core_sha256'], 'a'*64)

    def test_changed_staged_core_is_rejected(self):
        runtime, core, candidate = self.staged_core()
        core.write_bytes(b'another build')
        with self.assertRaisesRegex(ValueError, 'changed staged core'):
            package.runtime_identity(runtime, candidate)

    def test_staged_core_cannot_describe_another_runtime(self):
        runtime, _, candidate = self.staged_core()
        runtime['cine_sha256'] = 'c'*64
        with self.assertRaisesRegex(ValueError, 'another installed runtime'):
            package.runtime_identity(runtime, candidate)

    def test_archive_requires_explicit_runtime_status(self):
        self.manifest.pop('runtime_status')
        self.save_manifest()
        with self.assertRaisesRegex(ValueError, 'runtime status'):
            package.write_archive(self.files, self.root / 'ambiguous.zip', self.timestamp)


if __name__ == '__main__':
    unittest.main()

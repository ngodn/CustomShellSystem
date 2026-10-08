"""Exercise package replacement and interrupted rollback without a game installation."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('namespace_install', ROOT / 'tools/asset-paths/install_namespace_trial.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class NamespaceInstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root / 'Game'
        self.runtime = self.game / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
        self.runtime.mkdir(parents=True)
        self.old_selector = {'abi': 1, 'file': 'old.dll'}
        mod.atomic(self.runtime / 'core.json', self.old_selector)
        self.write(self.runtime / 'cores/old.dll', 'old core')
        state = self.runtime / 'state'
        self.write(state / 'profile.json', 'user settings')
        self.plan = {'game': str(self.game), 'old': {}, 'new': {},
            'previous_selector': self.old_selector, 'new_selector': {'abi': 1, 'file': 'new.dll'},
            'protected': {str(self.runtime / 'cores/old.dll'): mod.sha(self.runtime / 'cores/old.dll')},
            'protected_trees': {str(state): mod.directory_hashes(state)}, 'evidence': {}, 'stage_bytes': 4096}
        core = self.root / 'source/core.dll'
        self.write(core, 'new core')
        self.plan.update(core_source=str(core), core_sha256=mod.sha(core))
        for name in mod.NAMES + mod.RETIRED:
            self.plan['old'][name] = {}
            for suffix in mod.SUFFIXES:
                path = self.game / 'Content/Paks/~mods' / name / (name + suffix)
                self.write(path, 'old ' + name + suffix)
                self.plan['old'][name][path.name] = mod.sha(path)
        for name in mod.NAMES:
            self.plan['new'][name] = {}
            for suffix in mod.SUFFIXES:
                path = self.root / 'source' / (name + suffix)
                self.write(path, 'new ' + name + suffix)
                self.plan['new'][name][path.name] = {'source': str(path), 'sha256': mod.sha(path), 'bytes': path.stat().st_size}
        self.transaction = self.game / 'Content/namespace-test'

    def write(self, path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def baseline(self):
        self.assertEqual(mod.read(self.runtime / 'core.json'), self.old_selector)
        for name, hashes in self.plan['old'].items():
            mod.package_matches(self.game / 'Content/Paks/~mods' / name, hashes)
        mod.protected_unchanged(self.plan)

    def install(self, checkpoint=lambda _: None, closed=lambda: None):
        mod.install(self.plan, self.transaction, checkpoint=checkpoint, closed=closed)

    def test_install_and_repeatable_restore(self):
        self.install()
        self.assertTrue(mod.read(self.transaction / 'result.json')['installed'])
        mod.mounts(self.game, self.plan['new'])
        for name, rows in self.plan['new'].items():
            mod.package_matches(self.game / 'Content/Paks/~mods' / name, {n: r['sha256'] for n, r in rows.items()})
        # Restoring a trial must not replace profiles edited since installation.
        self.write(self.runtime / 'state/profile.json', 'new user settings')
        mod.restore(self.transaction, lambda: None)
        mod.restore(self.transaction, lambda: None)
        self.assertEqual((self.runtime / 'state/profile.json').read_text(), 'new user settings')
        self.assertEqual(mod.read(self.runtime / 'core.json'), self.old_selector)

    def test_failure_before_exchange(self):
        def fault(step):
            if step == 'staged': raise OSError('injected failure')
        with self.assertRaises(OSError): self.install(fault)
        self.baseline()

    def test_failure_after_retiring_one_package(self):
        def fault(step):
            if step == 'retired:' + mod.NAMES[0]: raise OSError('injected failure')
        with self.assertRaises(OSError): self.install(fault)
        self.baseline()

    def test_failure_after_mounting_one_package(self):
        def fault(step):
            if step == 'mounted:' + mod.NAMES[0]: raise OSError('injected failure')
        with self.assertRaises(OSError): self.install(fault)
        self.baseline()

    def test_failure_after_selector(self):
        def fault(step):
            if step == 'selected': raise OSError('injected failure')
        with self.assertRaises(OSError): self.install(fault)
        self.baseline()

    def test_game_launch_defers_rollback(self):
        running = False
        def closed():
            if running: raise RuntimeError('Game running')
        def launch(step):
            nonlocal running
            if step.startswith('retired:'): running = True
        with self.assertRaises(RuntimeError): self.install(launch, closed)
        self.assertTrue((self.transaction / 'backup' / mod.NAMES[0]).exists())
        running = False
        mod.restore(self.transaction, closed)
        self.baseline()

    def test_duplicate_container_rejected(self):
        self.write(self.game / 'Content/Paks/duplicate' / (mod.NAMES[0] + '.pak'), 'duplicate')
        with self.assertRaises(ValueError): self.install()
        self.assertFalse(self.transaction.exists())

    def test_changed_candidate_preserves_baseline(self):
        row = next(iter(self.plan['new'][mod.NAMES[0]].values()))
        self.write(Path(row['source']), 'unexpected edit')
        with self.assertRaises(ValueError): self.install()
        self.baseline()

    def test_protected_profile_change_rejected(self):
        self.write(self.runtime / 'state/profile.json', 'changed')
        with self.assertRaises(ValueError): self.install()
        self.assertFalse(self.transaction.exists())

    def test_game_running_before_staging(self):
        def running(): raise RuntimeError('Game running')
        with self.assertRaises(RuntimeError): self.install(closed=running)
        self.assertFalse(self.transaction.exists())
        self.baseline()

    def test_disk_space_checked_before_staging(self):
        with patch.object(mod.shutil, 'disk_usage') as disk:
            disk.return_value.free = 0
            with self.assertRaises(ValueError): self.install()
        self.assertFalse(self.transaction.exists())
        self.baseline()

    def test_changed_candidate_after_staging(self):
        def change(step):
            if step == 'staged':
                p = self.transaction / 'candidate' / mod.NAMES[0] / (mod.NAMES[0] + '.ucas')
                self.write(p, 'corrupt candidate')
        with self.assertRaises(ValueError): self.install(change)
        self.baseline()

    def test_unknown_mounted_changes_not_overwritten_by_restore(self):
        self.install()
        p = self.game / 'Content/Paks/~mods' / mod.NAMES[0] / (mod.NAMES[0] + '.ucas')
        self.write(p, 'another session changed this')
        with self.assertRaises(ValueError): mod.restore(self.transaction, lambda: None)
        self.assertEqual(p.read_text(), 'another session changed this')
        self.assertTrue((self.transaction / 'backup' / mod.NAMES[0]).exists())

    def test_existing_core_is_not_overwritten(self):
        def conflict(step):
            if step == 'staged': self.write(self.runtime / 'cores/new.dll', 'another build')
        with self.assertRaises(FileExistsError): self.install(conflict)
        self.assertEqual((self.runtime / 'cores/new.dll').read_text(), 'another build')
        self.baseline()


if __name__ == '__main__':
    unittest.main()

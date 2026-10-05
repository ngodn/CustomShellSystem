import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import update_performance_trial as installer


class TrialUpdateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.game = self.root / 'game'
        self.mod = self.game / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
        self.cine = self.game / 'Binaries/Win64/ue4ss/Mods/CSSX/extensions/eins0fx.cine'
        self.target = self.game / 'Content/Paks/~mods' / installer.NAME
        self.previous, self.pack, self.receipt = [self.root / p for p in ('previous', 'pack', 'receipt')]
        self.core, self.cine_dll, self.menu, self.delta = [self.root / p for p in
                                                       ('core.dll', 'cine.dll', 'menu.json', 'delta.json')]
        self.closed = patch.object(installer, 'processes', return_value=[])
        self.closed.start()
        self.addCleanup(self.closed.stop)
        ue4ss = self.game / 'Binaries/Win64/ue4ss/UE4SS.dll'
        self.write(ue4ss, 'ue4ss')
        expected = patch.object(installer, 'EXPECTED_UE4SS', installer.sha(ue4ss))
        expected.start()
        self.addCleanup(expected.stop)
        selector, extension = {'abi': 1, 'file': 'old-core.dll'}, {'api': 3, 'entry': 'old-cine.dll'}
        self.write(self.mod / 'core.json', selector)
        self.write(self.cine / 'extension.json', extension)
        for path, content in ((self.mod / 'cores/old-core.dll', 'old core'),
                              (self.cine / 'old-cine.dll', 'old cine'),
                              (self.mod / 'dlls/main.dll', 'loader'),
                              (self.mod / 'state/settings.json', 'saved appearance'),
                              (self.cine.parents[1] / 'state/take.json', 'saved take'),
                              (self.cine / 'menu.json', 'old menu'),
                              (self.previous / 'cine-menu.json', 'old menu'),
                              (self.core, 'new core'), (self.cine_dll, 'new cine'), (self.menu, 'new menu')):
            self.write(path, content)
        old_files, new_files = {}, {}
        for suffix in ('.pak', '.utoc', '.ucas'):
            name = installer.NAME + suffix
            self.write(self.target / name, 'old ' + suffix)
            self.write(self.pack / installer.NAME / name, 'new ' + suffix)
            old_files[name] = installer.sha(self.target / name)
            new_files[name] = installer.sha(self.pack / installer.NAME / name)
        self.write(self.previous / 'deployment.json', {
            'installed': True, 'asset_directory': str(self.target), 'assets': old_files,
            'previous_core': selector, 'previous_cine': extension,
            'core': 'old-core.dll', 'cine': 'old-cine.dll',
            'core_sha256': installer.sha(self.mod / 'cores/old-core.dll'),
            'cine_sha256': installer.sha(self.cine / 'old-cine.dll')})
        faces = ['face' + str(i) for i in range(6)]
        self.write(self.pack / 'verification.json', {'passed': True, 'exact_export_payloads': 68,
            'facial_meshes': faces, 'files': new_files, 'dependencies': {}, 'facial_evidence': {}})
        self.write(self.pack / 'facial-independent-readback.json',
                   {'passed': True, 'results': [{'target': f, 'morphs': [{'name':str(i)} for i in range(105)]}
                                               for f in faces]})
        self.write(self.delta, {'passed': True, 'source_deltas_checked': True,
            'results': [{'target': f, 'source_delta_comparison': {'passed': True,
                         'shapes': [{'name':str(i)} for i in range(105)], 'expressions': [None] * 21}} for f in faces]})
        self.original = self.game_files()

    @staticmethod
    def write(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data) if isinstance(data, dict) else data)

    def game_files(self):
        return {str(p.relative_to(self.game)): p.read_bytes() for p in self.game.rglob('*') if p.is_file()}

    def run_update(self, keep_cine=False):
        return installer.update(self.game, self.previous, self.pack, self.receipt,
                                self.core, self.cine_dll, self.menu, self.delta, keep_cine=keep_cine)

    def test_keep_cine_updates_only_css_and_assets(self):
        before={str(p):p.read_bytes() for p in self.cine.rglob('*') if p.is_file()}
        report=json.loads(self.run_update(keep_cine=True).read_text())
        self.assertTrue(report['cine_unchanged'])
        self.assertEqual(report['cine'], 'old-cine.dll')
        self.assertEqual(before,{str(p):p.read_bytes() for p in self.cine.rglob('*') if p.is_file()})

    def test_keep_cine_failure_never_removes_existing_extension(self):
        original_atomic=installer.atomic
        def fail_after_write(path,value):
            original_atomic(path,value)
            if path==self.mod/'core.json':
                raise OSError('simulated selector failure')
        with patch.object(installer,'atomic',side_effect=fail_after_write),self.assertRaises(OSError):
            self.run_update(keep_cine=True)
        self.assertEqual(self.game_files(),self.original)

    def test_mismatched_morph_names_are_rejected_before_writes(self):
        data=json.loads(self.delta.read_text())
        data['results'][0]['source_delta_comparison']['shapes'][0]['name']='different'
        self.write(self.delta,data)
        with self.assertRaises(ValueError):
            self.run_update()
        self.assertEqual(self.game_files(),self.original)

    def test_update_preserves_original_binaries_settings_and_asset_rollback(self):
        report = json.loads(self.run_update().read_text())
        self.assertTrue(report['installed'])
        self.assertFalse(report['live_verified'])
        installer.check_hashes(report['protected_hashes'])
        for name, digest in report['assets'].items():
            self.assertEqual(installer.sha(self.target / name), digest)
            self.assertEqual((Path(report['asset_rollback_directory']) / name).read_bytes(),
                             self.original[str((self.target / name).relative_to(self.game))])
        self.assertEqual((self.cine / 'menu.json').read_bytes(), self.menu.read_bytes())
        with self.assertRaises(ValueError):
            self.run_update()

    def test_running_game_and_changed_installed_file_are_rejected_before_writes(self):
        with patch.object(installer, 'processes', return_value=[42]), self.assertRaises(RuntimeError):
            self.run_update()
        self.assertEqual(self.game_files(), self.original)
        self.write(self.cine / 'menu.json', 'external update')
        before = self.game_files()
        with self.assertRaises(ValueError):
            self.run_update()
        self.assertEqual(self.game_files(), before)
        self.assertFalse(self.receipt.exists())

    def test_failure_after_selector_write_restores_complete_previous_trial(self):
        original_atomic = installer.atomic
        def fail_after_write(path, value):
            original_atomic(path, value)
            if path == self.mod / 'core.json':
                raise OSError('simulated selector write failure')
        with patch.object(installer, 'atomic', side_effect=fail_after_write), self.assertRaises(OSError):
            self.run_update()
        self.assertEqual(self.game_files(), self.original)
        self.assertTrue(json.loads((self.receipt / 'install-error.json').read_text())['rolled_back'])

    def test_failure_between_asset_renames_restores_previous_pack(self):
        original_rename = Path.rename
        def fail_stage(path, destination):
            if '-facial-stage-' in path.name:
                raise OSError('simulated asset promotion failure')
            return original_rename(path, destination)
        with patch.object(Path, 'rename', fail_stage), self.assertRaises(OSError):
            self.run_update()
        self.assertEqual(self.game_files(), self.original)

    def test_game_opening_mid_update_defers_rollback_instead_of_hot_reloading(self):
        calls = 0
        def game_state():
            nonlocal calls
            calls += 1
            return [42] if calls >= 4 else []
        with patch.object(installer, 'processes', side_effect=game_state), self.assertRaises(RuntimeError):
            self.run_update()
        error = json.loads((self.receipt / 'install-error.json').read_text())
        self.assertTrue(error['rollback_deferred'])
        self.assertTrue(error['old_assets_moved'])
        self.assertFalse(error['new_assets_moved'])
        self.assertEqual((self.mod / 'core.json').read_bytes(),
                         self.original[str((self.mod / 'core.json').relative_to(self.game))])


if __name__ == '__main__':
    unittest.main()

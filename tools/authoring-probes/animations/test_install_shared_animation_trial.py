import json
import unittest
from pathlib import Path

import install_shared_animation_trial as shared
import update_performance_trial as installer
import test_update_performance_trial as fixtures


class SharedTrialTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.TrialUpdateTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        f = self.fixture
        self.baseline = f.root / 'combined-verification.json'
        f.write(self.baseline, dict(passed=True, entries=63, exports=98, adapters=5,
            eve_performances=6, commander_performances=6, commander_controls=128, dependencies={}))
        verified = json.loads((f.pack / 'verification.json').read_text())
        verified.update(entries=63, exports=99, unchanged_existing_exports=96,
            replacements={'old1': 'new1', 'old2': 'new2'},
            protected={str(self.baseline): installer.sha(self.baseline)})
        f.write(f.pack / 'verification.json', verified)
        f.write(f.pack / 'host-validation.json', dict(passed=True, results=[{'exit_code': 0}] * 2))
        rows = []
        for index in range(57):
            report = f.root / 'motion' / (str(index) + '.json')
            f.write(report, {'passed': True})
            rows.append(dict(id=str(index), frames=1, tracks=1, report=str(report), sha256=installer.sha(report)))
        f.write(f.pack / 'original-clips-full-motion.json',
                dict(passed=True, clips=57, frames=57, transforms=57, results=rows))

    def run_update(self):
        f = self.fixture
        return installer.update(f.game, f.previous, f.pack, f.receipt, f.core,
            None, None, self.baseline, keep_cine=True, validator=shared.validate_shared_package)

    def test_combined_pack_replaces_assets_and_core_but_preserves_cine_and_state(self):
        f = self.fixture
        report = json.loads(self.run_update().read_text())
        self.assertTrue(report['installed'])
        self.assertFalse(report['live_verified'])
        self.assertEqual(report['cine'], 'old-cine.dll')
        for path, contents in f.original.items():
            if '/CSSX/' in path or '/state/' in path:
                self.assertEqual((f.game / path).read_bytes(), contents)
        rollback = Path(report['asset_rollback_directory'])
        self.assertEqual((rollback / (installer.NAME + '.ucas')).read_text(), 'old .ucas')

    def test_changed_provenance_report_is_rejected_before_game_writes(self):
        f = self.fixture
        baseline = json.loads(self.baseline.read_text())
        baseline['commander_performances'] = 5
        f.write(self.baseline, baseline)
        with self.assertRaises(ValueError):
            self.run_update()
        self.assertEqual(f.game_files(), f.original)
        self.assertFalse(f.receipt.exists())

    def test_changed_motion_report_is_rejected_even_if_still_marked_passed(self):
        f = self.fixture
        f.write(f.root / 'motion/0.json', {'passed': True, 'changed': True})
        with self.assertRaises(RuntimeError):
            self.run_update()
        self.assertEqual(f.game_files(), f.original)
        self.assertFalse(f.receipt.exists())

    def test_corrupt_candidate_container_is_rejected_before_game_writes(self):
        f = self.fixture
        f.write(f.pack / installer.NAME / (installer.NAME + '.ucas'), 'corrupt')
        with self.assertRaises(RuntimeError):
            self.run_update()
        self.assertEqual(f.game_files(), f.original)
        self.assertFalse(f.receipt.exists())


if __name__ == '__main__':
    unittest.main()

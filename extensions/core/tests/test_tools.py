"""Tool logic that must not touch a real game: migration planning and probe math."""
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import cssx_migrate  # noqa: E402
import fps_probe  # noqa: E402


class MigrationPlan(unittest.TestCase):
    def setUp(self):
        self.temp = Path(tempfile.mkdtemp(prefix='cssx-migrate-'))
        self.css = self.temp / 'CustomShellSystem'
        self.cssx = self.temp / 'CSSX'
        for rel in ('extensions/a.one/extension.json', 'extensions/b.two/extension.json', 'state/extensions/a.one.json',
                    'state/extensions/a.one.json.bak', 'logs/extensions/a.one/current.jsonl', 'logs/cssx.jsonl',
                    'output/extensions/a.one/report.txt', 'assets/cssx-logo.png', 'cssx.json', 'cores/cssx_core-0.3.1.dll',
                    'cores/cssx_core-dev-abc.dll', 'cores/css_core-1.0.0-alpha.1.dll', 'core.json', 'dlls/main.dll',
                    'state/state.json', 'catalog/x.css.json', 'enabled.txt'):
            path = self.css / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(rel)
        (self.cssx / 'extensions/b.two').mkdir(parents=True)

    def tearDown(self):
        shutil.rmtree(self.temp)

    def test_plan_moves_only_legacy_cssx_data(self):
        plan = cssx_migrate.plan(self.css, self.cssx)
        moved = {str(s.relative_to(self.css)) for s, _ in plan['moves']}
        self.assertEqual(moved, {'extensions/a.one', 'state/extensions/a.one.json', 'state/extensions/a.one.json.bak',
                                 'logs/extensions/a.one', 'logs/cssx.jsonl', 'output/extensions/a.one', 'assets/cssx-logo.png'})
        deleted = {str(d.relative_to(self.css)) for d in plan['deletes']}
        self.assertEqual(deleted, {'cssx.json', 'cores/cssx_core-0.3.1.dll', 'cores/cssx_core-dev-abc.dll'})
        self.assertTrue(any('b.two' in s for s in plan['skipped']))

    def test_perform_and_restore_round_trip(self):
        cssx_migrate.processes = lambda: []
        backup = cssx_migrate.perform(self.css, self.cssx, dry_run=False)
        self.assertTrue((self.cssx / 'extensions/a.one/extension.json').is_file())
        self.assertTrue((self.cssx / 'state/a.one.json').is_file())
        self.assertFalse((self.css / 'cssx.json').exists())
        self.assertTrue((self.css / 'cores/css_core-1.0.0-alpha.1.dll').is_file())
        self.assertTrue((self.css / 'state/state.json').is_file())
        self.assertTrue((self.css / 'core.json').is_file())
        manifest = json.loads((backup / 'manifest.json').read_text())
        self.assertEqual(len(manifest['entries']), 10)
        cssx_migrate.restore(self.css, self.cssx, backup.name)
        self.assertEqual((self.css / 'cssx.json').read_text(), 'cssx.json')
        self.assertTrue((self.css / 'extensions/a.one/extension.json').is_file())
        self.assertFalse((self.cssx / 'extensions/a.one').exists())

    def test_dry_run_changes_nothing(self):
        cssx_migrate.processes = lambda: []
        before = sorted(str(p.relative_to(self.temp)) for p in self.temp.rglob('*'))
        self.assertIsNone(cssx_migrate.perform(self.css, self.cssx, dry_run=True))
        after = sorted(str(p.relative_to(self.temp)) for p in self.temp.rglob('*'))
        self.assertEqual(before, after)


class ProbeMath(unittest.TestCase):
    def setUp(self):
        self.temp = Path(tempfile.mkdtemp(prefix='cssx-perf-'))
        fps_probe.WORK = self.temp

    def tearDown(self):
        shutil.rmtree(self.temp)

    def write(self, label, medians, p99=20.0):
        for i, m in enumerate(medians, 1):
            (self.temp / f'{label}-{i}.json').write_text(json.dumps({'stats': {'engine': {'median_ms': m, 'p99_ms': p99}}}))

    def test_within_noise(self):
        self.write('A', [13.0, 13.4, 13.2]); self.write('B', [13.3, 13.1, 13.5])
        self.assertEqual(fps_probe.compare('A', 'B')['verdict'], 'within noise')

    def test_slower(self):
        self.write('A', [13.0, 13.4, 13.2]); self.write('B', [16.3, 16.1, 16.5])
        result = fps_probe.compare('A', 'B')
        self.assertEqual(result['verdict'], 'candidate slower')
        self.assertAlmostEqual(result['median_delta_ms'], 3.1, places=6)

    def test_inconclusive_with_too_few_pairs(self):
        self.write('A', [13.0]); self.write('B', [16.0])
        self.assertEqual(fps_probe.compare('A', 'B')['verdict'], 'inconclusive')

    def test_p99_regression_flagged(self):
        self.write('A', [13.0, 13.4, 13.2], p99=18); self.write('B', [13.3, 13.1, 13.5], p99=40)
        self.assertEqual(fps_probe.compare('A', 'B')['verdict'], 'median within noise, p99 regressed')


if __name__ == '__main__':
    unittest.main()

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


class ReleaseVerify(unittest.TestCase):
    def setUp(self):
        import cssx_release
        self.rel = cssx_release
        self.temp = Path(tempfile.mkdtemp(prefix='cssx-rel-'))

    def tearDown(self):
        shutil.rmtree(self.temp)

    def make(self, files, manifest_name='CSSX/release.json', tamper=None):
        import zipfile
        meta = {'product': 'CSSX', 'version': '1.0.0', 'source_commit': 'abc', 'manifest_name': manifest_name,
                'files': {k: self.rel.digest(v) for k, v in files.items()}}
        path = self.temp / 'x.zip'
        with zipfile.ZipFile(path, 'w') as z:
            for k, v in files.items():
                z.writestr(k, v if not (tamper and k == tamper) else v + b'x')
            z.writestr(manifest_name, json.dumps(meta))
        return path

    def test_good_archive_verifies(self):
        path = self.make({'CSSX/enabled.txt': b'', 'CSSX/core.json': b'{}'})
        self.assertEqual(self.rel.verify(path)['version'], '1.0.0')

    def test_checksum_mismatch_rejected(self):
        path = self.make({'CSSX/core.json': b'{}'}, tamper='CSSX/core.json')
        with self.assertRaises(ValueError):
            self.rel.verify(path)

    def test_forbidden_members_rejected(self):
        for name in ('CSSX/dev/enabled.txt', 'CSSX/logs/cssx.jsonl', 'CSSX/UE4SS.dll', 'CSSX/core/css_core-1.dll', 'CSSX/state/x.json'):
            path = self.make({name: b'', 'CSSX/core.json': b'{}'})
            with self.assertRaises(ValueError, msg=name):
                self.rel.verify(path)

    def test_non_dll_named_dll_rejected(self):
        path = self.make({'CSSX/dlls/main.dll': b'not a dll'})
        with self.assertRaises(ValueError):
            self.rel.verify(path)

    def test_tag_export_excludes_uncommitted_files(self):
        import subprocess
        repo = self.temp / 'repo'
        repo.mkdir()
        def git(*args):
            subprocess.run(['git', '-c', 'user.name=Release test', '-c', 'user.email=release-test@example.invalid',
                            '-c', 'core.hooksPath=/dev/null', *args], cwd=repo, check=True, capture_output=True)
        git('init')
        (repo / 'README.txt').write_text('tagged text')
        git('add', 'README.txt'); git('commit', '-m', 'Test release source')
        git('tag', '-a', 'v1.0.0', '-m', 'Test release')
        (repo / 'README.txt').write_text('unfinished changes')
        (repo / 'scratch.txt').write_text('not for release')
        tree = self.temp / 'tagged'
        commit = self.rel.export_tag('v1.0.0', tree, repo)
        self.assertEqual(len(commit), 40)
        self.assertEqual((tree / 'README.txt').read_text(), 'tagged text')
        self.assertFalse((tree / 'scratch.txt').exists())
        self.assertEqual((repo / 'README.txt').read_text(), 'unfinished changes')
        git('tag', 'lightweight')
        with self.assertRaisesRegex(ValueError, 'annotated'):
            self.rel.export_tag('lightweight', self.temp / 'bad-tag', repo)

    def test_pair_has_both_changelogs_for_the_requested_versions(self):
        for product, version in [('cssx', '1.3.1'), ('cssx-cheat-menu', '1.1.2')]:
            path = self.temp / f'docs/nexus/{product}/changelog-v{version}.bbcode.txt'
            path.parent.mkdir(parents=True)
            path.write_text(product + ' tagged notes')
        notes = self.rel.pair_changelogs(self.temp, '1.3.1', '1.1.2')
        self.assertEqual(notes, {'changelog-v1.3.1.bbcode.txt': b'cssx tagged notes',
                                 'cheat-menu-changelog-v1.1.2.bbcode.txt': b'cssx-cheat-menu tagged notes'})
        with self.assertRaises(FileNotFoundError):
            self.rel.pair_changelogs(self.temp, '1.3.1', '1.1.3')

    def test_perf_tuner_package_contains_only_player_files(self):
        dll = bytearray(128)
        dll[:2] = b'MZ'
        dll[60:64] = (64).to_bytes(4, 'little')
        dll[64:70] = b'PE\0\0\x64\x86'
        dll[86:88] = (0x2000).to_bytes(2, 'little')
        (self.temp / 'perf_tuner.dll').write_bytes(dll)
        version, files = self.rel.perf_tuner_files(self.temp)
        base = 'eins0fx.perf-tuner/'
        self.assertEqual(set(files), {base + name for name in (
            'extension.json', 'perf_tuner.dll', 'tweaks.json', 'README.txt', 'THIRD_PARTY_NOTICES.txt')})
        manifest = json.loads(files[base + 'extension.json'])
        self.assertEqual(version, manifest['version'])
        self.assertEqual(manifest['entry'], 'perf_tuner.dll')
        self.assertEqual((manifest['schema'], manifest['api']), (2, 3))
        path = self.temp / 'perf-tuner.zip'
        self.rel.write_zip(path, files, {'product': 'CSSX Perf Tuner', 'version': version,
                                      'manifest_name': base + 'release.json'}, (2026, 10, 6, 0, 0, 0))
        self.assertEqual(self.rel.verify(path)['product'], 'CSSX Perf Tuner')
        self.assertTrue(path.with_name(path.name + '.sha256').is_file())

    def test_perf_tuner_reads_requested_source_tree(self):
        src = self.temp / 'tagged/extensions/perf-tuner'
        src.mkdir(parents=True)
        (src / 'extension.json').write_text(json.dumps({
            'id': 'eins0fx.perf-tuner', 'version': '9.8.7', 'files': ['README.txt']}))
        (src / 'README.txt').write_text('Text from the tagged tree')
        (self.temp / 'perf_tuner.dll').write_bytes(b'dll fixture')
        version, files = self.rel.perf_tuner_files(self.temp, self.temp / 'tagged')
        self.assertEqual(version, '9.8.7')
        self.assertEqual(files['eins0fx.perf-tuner/README.txt'], b'Text from the tagged tree')

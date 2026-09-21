"""Project layout invariants: version pins, manifests and documentation."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ProjectLayout(unittest.TestCase):
    def test_version_is_semver(self):
        version = (ROOT / 'VERSION').read_text().strip()
        self.assertRegex(version, r'^\d+\.\d+\.\d+$')

    def test_abi_header_pins(self):
        header = (ROOT / 'include/cssx/abi.h').read_text()
        self.assertIn('#define CSSX_ABI 3u', header)
        core = (ROOT / 'src/shared/core_abi.h').read_text()
        self.assertIn('#define CSSX_CORE_ABI 1u', core)

    def test_example_manifests_parse(self):
        for manifest in sorted((ROOT / 'examples').glob('*/extension.json')):
            data = json.loads(manifest.read_text())
            self.assertIn((data['schema'], data['api']), {(1, 1), (1, 2), (2, 3)}, manifest)
            self.assertRegex(data['id'], r'^[a-z][a-z0-9._-]*$')

    def test_cheat_menu_manifest_targets_abi3(self):
        data = json.loads((ROOT / 'cheat-menu/extension.json').read_text())
        self.assertEqual((data['schema'], data['api']), (2, 3))
        self.assertEqual(data['version'], (ROOT / 'VERSION').read_text().strip())

    def test_docs_present(self):
        for name in ('decisions.md', 'performance.md', 'migration.md', 'work-queue.md'):
            self.assertTrue((ROOT / 'docs' / name).is_file(), name)

    def test_runtime_pin_matches_repository(self):
        pin = json.loads((ROOT / '../../native/ue4ss-runtime.json').read_text())
        self.assertEqual(pin['revision'], '97b7e501')
        self.assertEqual(pin['dll_sha256'], 'fb1839ee91f71f83d508d44a2763a15ac1bb0c5fb4e504ac0fcfca64376a054a')


if __name__ == '__main__':
    unittest.main()

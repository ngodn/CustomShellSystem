"""The service contract is copied, not shared: CSS keeps its own copy so it never depends on
CSSX. This keeps the two copies identical apart from CSS's two-line provenance note."""
import unittest
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
REPO = CORE.parents[1]


class ServiceContract(unittest.TestCase):
    def test_css_copy_matches(self):
        published = (CORE / 'include/cssx/service.h').read_text().splitlines()
        copy = REPO / 'native/src/cssx_service.h'
        if not copy.exists():
            self.skipTest('CSS sources are not in this checkout')
        lines = copy.read_text().splitlines()
        self.assertEqual(lines[0], published[0])
        self.assertIn('Copy of the published CSSX service contract', lines[1])
        self.assertEqual(lines[3:], published[1:])


if __name__ == '__main__':
    unittest.main()

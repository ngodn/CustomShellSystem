import copy
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from css_sources import ready


class SourceStabilityTests(unittest.TestCase):
    def sample(self):
        return {'.': {'directory': True, 'bytes': 0, 'mtime_ns': 0, 'ctime_ns': 0},
                'mod.zip': {'directory': False, 'bytes': 10, 'mtime_ns': 0, 'ctime_ns': 0}}

    def test_age_includes_copy_ctime_and_directory(self):
        for entry in ('.', 'mod.zip'):
            for field in ('mtime_ns', 'ctime_ns'):
                state = self.sample(); state[entry][field] = 41_000_000_000
                self.assertFalse(ready(state, state, 100_000_000_000)[0])
        state = self.sample()
        self.assertTrue(ready(state, state, 60_000_000_000)[0])

    def test_changes_partial_and_missing_companions(self):
        state = self.sample(); changed = copy.deepcopy(state)
        changed['mod.zip']['bytes'] += 1
        self.assertFalse(ready(state, changed, 100_000_000_000)[0])
        for filename in ('copy.part', 'mod.utoc', 'mod.ucas'):
            changed = copy.deepcopy(state); changed[filename] = changed.pop('mod.zip')
            self.assertFalse(ready(changed, changed, 100_000_000_000)[0])

    def test_age_cannot_be_disabled(self):
        with self.assertRaises(ValueError):
            ready({}, {}, 0, minimum_age=0)


if __name__ == '__main__':
    unittest.main()

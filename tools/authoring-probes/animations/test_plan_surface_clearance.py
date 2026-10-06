import unittest

import numpy as np

from plan_surface_clearance import clearance_envelope


class ClearanceTests(unittest.TestCase):
    def test_between_key_penetration(self):
        heights = np.full(33, 2.)
        heights[14] = -20
        keys, dense = clearance_envelope(heights, 4)
        self.assertEqual(len(keys), 9)
        self.assertGreaterEqual(float((heights + dense).min()), .01 - 1e-9)
        self.assertTrue(np.all(keys >= 0))

    def test_clear_surface_stays_unchanged(self):
        for substeps in (1, 2, 4):
            keys, dense = clearance_envelope(np.full(8 * substeps + 1, 3.), substeps)
            np.testing.assert_array_equal(keys, np.zeros(9))
            np.testing.assert_array_equal(dense, np.zeros(8 * substeps + 1))

    def test_endpoints_and_short_sequence(self):
        for heights in ([-4, 1], [1, -4], [-4, -4]):
            _, dense = clearance_envelope(heights, 1)
            self.assertGreaterEqual(float((np.asarray(heights) + dense).min()), .01 - 1e-9)

    def test_invalid_clock_or_values(self):
        for heights, substeps in (([1, 1], 4), ([1, 2], 3), ([1, float('nan')], 1),
                                  ([1], 1), ([[1, 2]], 1)):
            with self.subTest(heights=heights, substeps=substeps):
                with self.assertRaises(ValueError):
                    clearance_envelope(heights, substeps)


if __name__ == '__main__':
    unittest.main()

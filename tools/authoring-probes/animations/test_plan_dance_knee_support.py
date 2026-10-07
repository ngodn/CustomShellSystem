import unittest

import numpy as np

from plan_dance_knee_support import directions


def leg(flex, signs=None):
    angle = np.radians(np.asarray(flex) / 2)
    signs = np.ones(len(angle)) if signs is None else np.asarray(signs)
    hip = np.zeros((len(angle), 3))
    knee = np.column_stack((40 * np.sin(angle) * signs, np.zeros(len(angle)), -40 * np.cos(angle)))
    foot = np.column_stack((np.zeros(len(angle)), np.zeros(len(angle)), -80 * np.cos(angle)))
    rotation = np.tile([0., 0., 0., 1.], (len(angle), 1))
    scale = np.ones((len(angle), 3))
    return [(p, rotation, scale) for p in (hip, knee, foot)]


class KneeSupportTests(unittest.TestCase):
    names = ['thigh_l', 'calf_l', 'foot_l']

    def test_airborne_and_bent_poses_remain_unmodified(self):
        for flex, heights in [([30, 2, 2, 30], [1.] * 4), ([30, 40, 30], [-3.] * 3)]:
            result, repairs = directions(leg(flex), self.names, 'l', heights)
            np.testing.assert_array_equal(result, np.zeros((len(flex), 3)))
            self.assertEqual(repairs, [])

    def test_nearly_straight_reversal_uses_measured_forward_anchors(self):
        result, repairs = directions(leg([30, 2, 0, 2, 30], [1, -1, -1, -1, 1]),
                                     self.names, 'l', [-3.] * 5)
        np.testing.assert_allclose(np.array(result)[1:4], np.tile([1., 0., 0.], (3, 1)))
        np.testing.assert_array_equal(np.array(result)[[0, 4]], np.zeros((2, 3)))
        self.assertEqual(repairs[0]['anchors'], [0, 4])

    def test_start_of_clip_uses_first_measured_anchor(self):
        result, repairs = directions(leg([2, 2, 30], [-1, -1, 1]), self.names, 'l', [-3.] * 3)
        np.testing.assert_allclose(np.array(result)[:2], np.tile([1., 0., 0.], (2, 1)))
        self.assertEqual(repairs[0]['anchors'], [2, 2])

    def test_conflicting_or_absent_anchors_fail(self):
        with self.assertRaisesRegex(ValueError, '45 degrees'):
            directions(leg([30, 2, 30], [1, 1, -1]), self.names, 'l', [-3.] * 3)
        with self.assertRaisesRegex(ValueError, 'No measured'):
            directions(leg([2, 2]), self.names, 'l', [-3.] * 2)


if __name__ == '__main__':
    unittest.main()

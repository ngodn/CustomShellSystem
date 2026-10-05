"""Timing and quaternion regressions for shared animation preparation."""
import math
import unittest

import numpy as np

from review_source_tracks import sample
from sample_timeline import timeline


class TimelineTests(unittest.TestCase):
    def test_30_to_60_preserves_every_source_sample_and_both_endpoints(self):
        clock = timeline(381, 12.666667, 60)
        self.assertEqual(clock.frames, 761)
        self.assertEqual(clock.fps, 60)
        self.assertEqual([clock.source_frame(i) for i in range(0, 761, 2)], list(range(381)))
        self.assertEqual(clock.source_frame(1), .5)
        self.assertEqual((clock.frames - 1) / clock.fps, 380 / 30)

    def test_native_60_does_not_duplicate_or_drop_samples(self):
        for count, seconds in ((3943, 65.7), (1001, 16.666666)):
            clock = timeline(count, seconds)
            self.assertEqual(clock.frames, count)
            self.assertEqual([clock.source_frame(i) for i in range(count)], list(range(count)))

    def test_legacy_30_is_unchanged(self):
        clock = timeline(201, 6.6666665)
        self.assertEqual(clock.frames, 201)
        self.assertEqual(clock.fps, 30)
        self.assertEqual([clock.source_frame(i) for i in range(201)], list(range(201)))

    def test_rejects_retiming_downsampling_and_invalid_dimensions(self):
        for args in ((201, 7), (201, 6.5), (201, math.nan), (201, math.inf),
                     (201, 0), (1, 1), (100001, 1), (201.0, 6.6666665),
                     (121, 2, 30), (61, 1, 24)):
            with self.subTest(args=args), self.assertRaises(ValueError):
                timeline(*args)

    def test_rejects_samples_outside_clip(self):
        clock = timeline(61, 2, 60)
        for index in (-1, 121, .5):
            with self.subTest(index=index), self.assertRaises(ValueError):
                clock.source_frame(index)


class InterpolationTests(unittest.TestCase):
    def test_uneven_key_times_preserve_motion_timing(self):
        bone = dict(translations=[dict(X=0, Y=0, Z=0), dict(X=6, Y=0, Z=0),
                                  dict(X=10, Y=0, Z=0)], translationTimes=[0, 3, 10])
        value = sample(bone, 'translations', 'translationTimes', 'Translation', 1.5, 11)
        np.testing.assert_array_equal(value, [3, 0, 0])
        value = sample(bone, 'translations', 'translationTimes', 'Translation', 6.5, 11)
        np.testing.assert_array_equal(value, [8, 0, 0])

    def test_quaternion_short_arc_and_normalization(self):
        bone = dict(rotations=[dict(X=0, Y=0, Z=0, W=1),
                               dict(X=0, Y=0, Z=-math.sqrt(.5), W=-math.sqrt(.5))],
                    rotationTimes=[0, 2])
        value = sample(bone, 'rotations', 'rotationTimes', 'Rotation', 1, 3)
        np.testing.assert_allclose(value, [0, 0, math.sin(math.pi/8), math.cos(math.pi/8)])
        self.assertAlmostEqual(float(np.linalg.norm(value)), 1)

    def test_opposite_quaternion_signs_do_not_spin(self):
        bone = dict(rotations=[dict(X=0, Y=0, Z=0, W=1), dict(X=0, Y=0, Z=0, W=-1)],
                    rotationTimes=[])
        for frame in (0, .5, 1, 1.5, 2):
            np.testing.assert_array_equal(
                sample(bone, 'rotations', 'rotationTimes', 'Rotation', frame, 3), [0, 0, 0, 1])

    def test_empty_channel_uses_bind_pose(self):
        bone = dict(scales=[], scaleTimes=[], reference=dict(Scale3D=dict(X=1, Y=2, Z=1)))
        np.testing.assert_array_equal(sample(bone, 'scales', 'scaleTimes', 'Scale3D', .5, 3), [1, 2, 1])

    def test_single_key_remains_constant(self):
        bone = dict(translations=[dict(X=3, Y=4, Z=5)], translationTimes=[])
        for frame in (0, .5, 60):
            np.testing.assert_array_equal(
                sample(bone, 'translations', 'translationTimes', 'Translation', frame, 61), [3, 4, 5])


if __name__ == '__main__':
    unittest.main()

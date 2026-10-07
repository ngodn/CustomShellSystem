"""Exercise reduction with cancelling morphs and runtime timestamp collisions."""
import unittest

import numpy as np

from compile_sampled_face import compile_samples, geometry_errors, interpolate


class SampledFaceTests(unittest.TestCase):
    def test_preserves_reversal_and_cancelling_controls(self):
        times = np.linspace(0, 3, 181)
        values = np.stack((np.sin(times*4), np.sin(times*4)*.9+times*.01), axis=1)
        shapes = np.array([[[1., 0, 0], [0, 2., 0]], [[-1., 0, 0], [0, -1.9, 0]]])
        out_times, out_values, report = compile_samples(times, values, shapes)
        errors = geometry_errors(interpolate(out_times, out_values, times)-values, shapes)
        self.assertLessEqual(errors.max(), .001)
        self.assertLess(len(out_times), len(times))
        self.assertGreater(len(out_times), 2)
        self.assertLessEqual(report['conversion_bound_cm'], .001)

    def test_merges_colliding_float_times_without_duplicate_keys(self):
        times = np.array([0., 1., 1.+1e-9, 2.])
        values = times[:, None]*.1
        shapes = np.array([[[1., 0, 0]]])
        out_times, out_values, report = compile_samples(times, values, shapes)
        self.assertEqual(report['runtime_samples'], 3)
        self.assertEqual(len(out_times), 2)
        self.assertTrue(np.all(np.diff(out_times.astype(np.float32)) > 0))
        self.assertLess(geometry_errors(interpolate(out_times, out_values, times)-values, shapes).max(), 1e-7)

    def test_rejects_motion_lost_at_float_precision(self):
        with self.assertRaises(ValueError):
            compile_samples([0, 1, 1+1e-9, 2], [[0], [0], [1], [1]], [[[1, 0, 0]]])
        with self.assertRaises(ValueError):
            compile_samples([0, 0], [[0], [1]], [[[1, 0, 0]]])


if __name__ == '__main__':
    unittest.main()

import unittest

import numpy as np

from compile_sparse_sampled_face import check_runtime_limits, compile_sparse, sample_curves
from compile_sampled_face import geometry_errors


class SparseFaceTests(unittest.TestCase):
    def test_independent_curves_preserve_staggered_motion(self):
        times = np.linspace(0, 4, 401)
        values = np.stack((np.maximum(0, 1-abs(times-1)), np.maximum(0, 1-abs(times-3)), times*.1), axis=1)
        shapes = np.eye(3).reshape(3, 1, 3)
        curves, report = compile_sparse(times, values, shapes, .0001)
        check_runtime_limits(curves)
        self.assertLess(sum(map(len, curves)), 20)
        self.assertLess(geometry_errors(sample_curves(curves, times)-values, shapes).max(), .0001)
        self.assertLess(report['conversion_bound_cm'], .0001)
        self.assertNotEqual(curves[0][:, 0].tolist(), curves[1][:, 0].tolist())

    def test_checks_combined_error_instead_of_only_individual_curves(self):
        times = np.linspace(0, 2, 101)
        values = np.tile((.005*np.sin(times*np.pi/2))[:, None], (1, 3))
        shapes = np.ones((3, 1, 3))
        with self.assertRaises(ValueError):
            compile_sparse(times, values, shapes, .01)

    def test_parser_key_count_and_zero_curves(self):
        dense = np.stack((np.linspace(0, 1, 16384), np.ones(16384)), axis=1)
        check_runtime_limits([dense]*4)
        with self.assertRaises(ValueError):
            check_runtime_limits([dense]*5)
        with self.assertRaises(ValueError):
            check_runtime_limits([dense*0])

    def test_rejects_float_precision_loss_and_invalid_data(self):
        for times, values in [([0, 1, 1+1e-9, 2], [[0], [0], [1], [1]]),
                              ([0, 0], [[0], [1]]), ([0, 1], [[0], [float('nan')]])]:
            with self.assertRaises(ValueError):
                compile_sparse(times, values, [[[1, 0, 0]]], .001)


if __name__ == '__main__':
    unittest.main()

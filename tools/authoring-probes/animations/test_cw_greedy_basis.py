import unittest

import numpy as np

from cw_facial_basis import compact_basis


class GreedyBasisTests(unittest.TestCase):
    def test_long_clip_with_rare_local_detail(self):
        rng = np.random.default_rng(1729)
        basis = rng.normal(size=(6, 40, 3))
        weights = rng.normal(size=(1100, 6))
        samples = np.einsum('ij,jkl->ikl', weights, basis)
        samples[617, 39, 2] += .3
        shapes, curves, error = compact_basis(samples, .0001, 7, method='greedy')
        reconstructed = np.einsum('ij,jkl->ikl', curves, shapes)
        self.assertLessEqual(error, .0001)
        self.assertLessEqual(np.linalg.norm(samples-reconstructed, axis=2).max(), .0001)
        self.assertLessEqual(abs(curves).max(), 1.)
        with self.assertRaises(ValueError):
            compact_basis(samples, .0001, 6, method='greedy')

    def test_deterministic_and_zero(self):
        samples = np.arange(240, dtype=float).reshape(20, 4, 3)/20
        first = compact_basis(samples, .001, 3, method='greedy')
        second = compact_basis(samples, .001, 3, method='greedy')
        for a, b in zip(first, second):
            np.testing.assert_array_equal(a, b)
        shapes, weights, error = compact_basis(np.zeros((20, 4, 3)), .001, 3, method='greedy')
        self.assertEqual(error, 0.)
        self.assertFalse(shapes.any() or weights.any())

    def test_invalid_method(self):
        with self.assertRaises(ValueError):
            compact_basis(np.ones((3, 4, 3)), .01, 2, method='unknown')


if __name__ == '__main__':
    unittest.main()

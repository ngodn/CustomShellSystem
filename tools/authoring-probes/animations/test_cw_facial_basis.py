"""Exercise measured compression limits and independent correction recovery."""
import unittest
import numpy as np
from cw_facial_basis import compact_basis, filtered_shapes


class BasisTests(unittest.TestCase):
    def test_recovers_independent_mixtures(self):
        rng=np.random.default_rng(702)
        original=rng.normal(size=(3,8,3))
        weights=rng.normal(size=(20,3))
        samples=np.einsum('ij,jkl->ikl',weights,original)
        basis,coefficients,error=compact_basis(samples,1e-10,3)
        self.assertEqual(len(basis),3)
        self.assertLessEqual(np.abs(coefficients).max(),1)
        self.assertLess(error,1e-10)
        self.assertTrue(np.allclose(np.einsum('ij,jkl->ikl',coefficients,basis),samples))

    def test_insufficient_shape_budget_fails(self):
        samples=np.eye(6).reshape((6,2,3))
        with self.assertRaises(ValueError):compact_basis(samples,1e-5,5)

    def test_uses_worst_vertex_not_mean_error(self):
        samples=np.zeros((2,1000,3))
        samples[0,0,0]=1
        samples[1,999,2]=.1
        with self.assertRaises(ValueError):compact_basis(samples,.01,1)

    def test_rejects_nonfinite_data(self):
        with self.assertRaises(ValueError):compact_basis(np.full((2,3,3),np.nan),.01,2)

    def test_scaled_morphs_preserve_small_corrections(self):
        samples=np.array([[[.01,0,0],[.0001,0,0]]])
        filtered,gain=filtered_shapes(samples,.015,32)
        self.assertEqual(gain[0],32)
        self.assertEqual(filtered[0,0,0],.01)
        self.assertEqual(filtered[0,1,0],0)
        ordinary,_=filtered_shapes(samples,.015,1)
        self.assertFalse(ordinary.any())

    def test_gain_caps_authored_displacement(self):
        samples=np.array([[[0.,0.,2.]]])
        filtered,gain=filtered_shapes(samples,0,32)
        self.assertEqual(gain[0],8)
        self.assertTrue(np.array_equal(filtered,samples))


if __name__=='__main__':unittest.main()

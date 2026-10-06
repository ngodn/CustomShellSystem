import unittest

import numpy as np

from compile_facial_control import compile_control


class FacialControlTests(unittest.TestCase):
    def test_cubic_motion_and_direction_reversals(self):
        levels = np.array([.25,.5,.75,1.])
        gains = np.array([4.,3.,2.,1.])
        shapes = np.zeros((4,2,3))
        shapes[:,0,0] = 3*levels**3
        shapes[:,1,1] = 2*levels**2
        shapes *= gains[:,None,None]
        keys = np.array([[0,0],[.2,1],[.27,.1],[.7,.1],[1,.8]])
        times, curves, report = compile_control(keys,shapes,gains)
        query = np.linspace(0,1,10001)
        control = np.interp(query,keys[:,0],keys[:,1])
        output = np.stack([np.interp(query,times,curves[:,i]) for i in range(4)],axis=1)
        actual = np.einsum('ij,jkl->ikl',output,shapes)
        expected = np.zeros_like(actual)
        expected[:,0,0], expected[:,1,1] = 3*control**3, 2*control**2
        self.assertLessEqual(np.linalg.norm(actual-expected,axis=2).max(),report['conversion_bound_cm']+1e-8)
        self.assertLessEqual(report['conversion_bound_cm'],.00101)
        self.assertEqual(times[0],0)
        self.assertEqual(times[-1],1)

    def test_constant_motion_reduces_to_endpoints(self):
        shapes = np.ones((4,2,3))
        times,_,_ = compile_control([[0,.4],[.1,.4],[.7,.4],[1,.4]],shapes,np.ones(4))
        np.testing.assert_array_equal(times,[0,1])

    def test_unsupported_inputs_are_rejected(self):
        for keys in ([[0,0],[0,1]],[[0,0],[1,1.1]],[[0,0],[1,float('nan')]],[[0,0]]):
            with self.subTest(keys=keys), self.assertRaises(ValueError):
                compile_control(keys,np.ones((4,2,3)),np.ones(4))


if __name__ == '__main__':
    unittest.main()

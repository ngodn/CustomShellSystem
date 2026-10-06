"""Check the offline interpolation basis independently of facial sample data."""
import unittest

import numpy as np

from audit_cw_facial_interpolation import interpolation_weights


class InterpolationTests(unittest.TestCase):
    def test_cubic_reproduces_polynomials_on_nonuniform_knots(self):
        for axis in ((0, .25, .5, .75, 1), (0, .1, .3, .8, 1)):
            for value in np.linspace(0, 1, 101):
                for degree in range(4):
                    actual = sum(weight * knot**degree for knot, weight in
                                 interpolation_weights(axis, value, 'cubic'))
                    self.assertAlmostEqual(actual, value**degree, places=12)

    def test_cubic_preserves_samples_and_continuous_velocity(self):
        axis = (0, .25, .5, .75, 1)
        samples = np.array([0., .2, -.1, .4, 1.])

        def evaluate(value):
            return sum(weight * sample for (_, weight), sample in
                       zip(interpolation_weights(axis, value, 'cubic'), samples))

        for knot, sample in zip(axis, samples):
            self.assertAlmostEqual(evaluate(knot), sample, places=12)
        step = 1e-6
        for knot in axis[1:-1]:
            left = (evaluate(knot) - evaluate(knot - step)) / step
            right = (evaluate(knot + step) - evaluate(knot)) / step
            self.assertAlmostEqual(left, right, delta=1e-4)

    def test_does_not_extrapolate(self):
        for value in (-.01, 1.01, float('nan'), float('inf')):
            with self.assertRaises(ValueError):
                interpolation_weights((0, .25, .5, .75, 1), value, 'cubic')

    def test_linear_keeps_local_weights(self):
        self.assertEqual(interpolation_weights((0, .25, .5, .75, 1), .375, 'linear'),
                         [(0, 0), (.25, .5), (.5, .5), (.75, 0), (1, 0)])

    def test_split_quadratic_preserves_half_strength_slope_change(self):
        axis=(0,.25,.5,.75,1)
        def source(x):
            return x*x if x<=.5 else .25+3*(x-.5)**2
        for value in np.linspace(0,1,101):
            actual=sum(weight*source(knot) for knot,weight in
                       interpolation_weights(axis,value,'split-quadratic'))
            self.assertAlmostEqual(actual,source(value),places=12)


if __name__ == '__main__':
    unittest.main()

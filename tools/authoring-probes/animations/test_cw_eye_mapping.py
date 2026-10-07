"""Check gaze recovery against known rotations, including signed combinations."""
import unittest

import numpy as np

from cw_eye_mapping import blended_direction, gaze, rigid_rotation, scaled_rotation, solve_gaze


def rotation(axis, degrees):
    axis = np.asarray(axis, dtype=float)
    axis /= np.linalg.norm(axis)
    x, y, z = axis
    skew = np.array([[0, -z, y], [z, 0, -x], [-y, x, 0]])
    theta = np.radians(degrees)
    return np.eye(3) + np.sin(theta)*skew + (1-np.cos(theta))*(skew@skew)


class EyeMappingTests(unittest.TestCase):
    def test_fit_recovers_rotation_without_translation(self):
        points = np.random.default_rng(720).normal(size=(100, 3))
        expected = rotation([1, 2, 3], 40)
        actual, error = rigid_rotation(points, points@expected + [2, 8, -1])
        np.testing.assert_allclose(actual, expected, atol=1e-12)
        self.assertLess(error, 1e-12)
        with self.assertRaises(ValueError):
            rigid_rotation(points, points*[-1, 1, 1])
        with self.assertRaises(ValueError):
            rigid_rotation(np.zeros((10, 3)), np.zeros((10, 3)))

    def test_signed_rotation_and_derivative(self):
        axis = rotation([0, 0, 1], 30)
        for strength in (-1.4, -.4, 0, .7, 1.4):
            value, derivative = scaled_rotation(axis, strength)
            np.testing.assert_allclose(value, rotation([0, 0, 1], 30*strength), atol=1e-12)
            numeric = (scaled_rotation(axis, strength+1e-5)[0] -
                       scaled_rotation(axis, strength-1e-5)[0])/2e-5
            np.testing.assert_allclose(derivative, numeric, atol=1e-10)

    def test_gaze_solves_both_orders_at_signed_extremes(self):
        axes = [rotation([0, 0, 1], 30), rotation([1, 0, 0], 20)]
        for order in ('horizontal-vertical', 'vertical-horizontal'):
            for h in (-1.3, -.5, 0, .5, 1.3):
                for v in (-1.3, -.5, 0, .5, 1.3):
                    direction, _ = gaze(axes, order, [h, v])
                    np.testing.assert_allclose(solve_gaze(axes, order, direction), [h, v], atol=1e-8)
        with self.assertRaises(ValueError):
            solve_gaze(axes, 'horizontal-vertical', [0, -2, 0])

    def test_linear_morph_blend_is_not_linear_angle(self):
        turn = rotation([0, 0, 1], 40)
        direction = blended_direction([turn], [.25])
        expected_angle = np.arctan2(.25*np.sin(np.radians(40)), .75+.25*np.cos(np.radians(40)))
        expected = np.array([0., -1., 0.])@rotation([0, 0, 1], np.degrees(expected_angle))
        np.testing.assert_allclose(direction, expected, atol=1e-12)
        self.assertGreater(abs(np.degrees(expected_angle)-10), .3)
        with self.assertRaises(ValueError):
            blended_direction([rotation([0, 0, 1], 180)], [.5])


if __name__ == '__main__':
    unittest.main()

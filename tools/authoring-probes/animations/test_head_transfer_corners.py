"""Run with Blender --background --factory-startup --python-exit-code 1 --python FILE."""
import sys
import unittest
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
from transfer_author_head_shapes import corner_weights, incident


class Corners(unittest.TestCase):
    def setUp(self):
        self.source = np.array([[0,0,0],[1,0,0],[0,1,0],
                                [0,0,0],[1,0,0],[0,0,1]], dtype=float)
        self.faces = np.array([[0,1,2],[3,4,5]])

    def weights(self, target, faces):
        return corner_weights(0, [0,3], target, faces, incident(faces,len(target)),
            self.source, self.faces, incident(self.faces,len(self.source)))

    def test_distinct_surface_uses_its_own_delta(self):
        for face, expected in ((0,[1,0]),(1,[0,1])):
            points = self.source[self.faces[face]]
            np.testing.assert_allclose(self.weights(points,np.array([[0,1,2]])),expected)

    def test_welded_equal_corners_average_both_surfaces(self):
        points = np.array([[0,0,0],[1,0,0],[0,1,0],[0,0,1]],dtype=float)
        np.testing.assert_allclose(self.weights(points,np.array([[0,1,2],[0,1,3]])),[.5,.5])

    def test_welded_unequal_corners_use_angle(self):
        points = np.array([[0,0,0],[1,0,0],[0,1,0],[1,0,1]],dtype=float)
        np.testing.assert_allclose(self.weights(points,np.array([[0,1,2],[0,1,3]])),[2/3,1/3])

    def test_isolated_vertex_rejected(self):
        with self.assertRaises(ValueError):
            self.weights(self.source[:3],np.empty((0,3),dtype=int))


if __name__ == '__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Corners))
    if not result.wasSuccessful():
        raise RuntimeError('Head transfer corner tests failed')

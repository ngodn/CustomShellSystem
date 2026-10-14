"""Negative controls for surface correspondence and lossless corner reordering."""
import copy
import unittest

import numpy as np

from check_import_cooked_surface import compare
from prepare_eve_corner_order import reorder, verify


class SurfaceCorrespondence(unittest.TestCase):
    def setUp(self):
        self.source = dict(points=[[0, 0, 0], [1, 0, 0], [0, 1, 0]], uv_channels=3,
            wedges=[[0, 0, 0, .1, .2, .3, .4], [1, 1, 0, .5, .6, .7, .8], [2, 0, 1, .9, 1, 1.1, 1.2]],
            normals=[[0, 0, 1]]*3, colors=[[10, 20, 30, 255]]*3,
            faces=[[2, 0, 1, 0]], materials=['cloth'], influences=[[0, 0, 1]], morph_targets=[])
        self.surface = np.zeros(3, dtype=[('normal', '<f4', (4,)), ('tangent', '<f4', (4,)),
                                          ('uv', '<f4', (3, 2)), ('color', 'u1', (1, 4))])
        self.surface['normal'] = [1/255, 1/255, 1, 1]
        self.surface['tangent'] = [1, 1/255, 1/255, 1]
        self.surface['uv'] = np.asarray(self.source['wedges'])[:, 1:].reshape(3, 3, 2)
        self.surface['color'][:, 0] = self.source['colors']
        self.indices = np.asarray([2, 0, 1], dtype='<u4')
        self.sections = [dict(FirstIndex=0, NumFaces=1, MaterialIndex=0)]

    def check(self, source=None, surface=None, indices=None):
        return compare(self.source if source is None else source, np.asarray(self.source['points'], dtype='<f4'),
                       self.surface if surface is None else surface, self.indices if indices is None else indices, self.sections)

    def test_control_and_reordered_source_pass(self):
        self.assertTrue(self.check()['passed'])
        source = reorder(self.source, '/Game/Private/SK_Test', '/Game/Private/SKEL_Test')
        # Add package names to the control to reproduce the actual interchange.
        original = dict(self.source, mesh_package='old', skeleton_package='old')
        verify(original, source)
        self.assertTrue(self.check(source=source)['passed'])

    def test_each_uv_channel_change_fails(self):
        for channel in range(3):
            with self.subTest(channel=channel):
                changed = self.surface.copy()
                changed['uv'][0, channel, 0] += .25
                self.assertFalse(self.check(surface=changed)['passed'])

    def test_color_change_fails(self):
        changed = self.surface.copy()
        changed['color'][0, 0, 0] += 1
        self.assertFalse(self.check(surface=changed)['passed'])

    def test_flipped_normal_fails(self):
        changed = self.surface.copy()
        changed['normal'][0, 2] = -1
        self.assertFalse(self.check(surface=changed)['passed'])

    def test_reversed_winding_fails(self):
        self.assertFalse(self.check(indices=self.indices[::-1])['passed'])

    def test_corrupt_reorder_fails(self):
        original = dict(self.source, mesh_package='old', skeleton_package='old')
        changed = copy.deepcopy(reorder(original, 'new', 'new'))
        changed['wedges'][0][1] += .25
        with self.assertRaises(ValueError):
            verify(original, changed)

    def test_nan_is_rejected(self):
        changed = self.surface.copy()
        changed['normal'][0, 0] = float('nan')
        with self.assertRaises(ValueError):
            self.check(surface=changed)


if __name__ == '__main__':
    unittest.main()

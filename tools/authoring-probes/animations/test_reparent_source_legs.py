"""World-space preservation and failure cases for source leg hierarchy conversion."""
import copy
import math
import unittest

import numpy as np

from reparent_source_legs import derive, transforms


def fixture():
    return [dict(name=name, parent=parent, translation=p, rotation=[0, 0, 0, 1], scale=[1, 1, 1])
            for name, parent, p in [
                ('Bip001-Pelvis', -1, [0, 0, 100]),
                ('Bip001-Spine', 0, [0, 0, 10]),
                ('Bip001-L-Thigh', 1, [10, 0, -10]),
                ('Bip001-L-Foot', 2, [0, 0, -90]),
                ('Bip001-R-Thigh', 1, [-10, 0, -10]),
                ('Bip001-R-Foot', 4, [0, 0, -90])]]


class ReparentTests(unittest.TestCase):
    def test_animated_parent_motion_survives(self):
        bones = fixture()
        keys = [copy.deepcopy(bones[1]) for _ in range(2)]
        keys[1]['rotation'] = [0, 0, math.sqrt(.5), math.sqrt(.5)]
        tracks = [dict(name='Bip001-Spine', keys=keys)]
        before = copy.deepcopy((bones, tracks))
        derived, animation, _ = derive(bones, tracks, 2)
        world = transforms(derived, animation, 2)
        np.testing.assert_allclose(world[3][0], [[10, 0, 10], [0, 10, 10]], atol=1e-12)
        np.testing.assert_allclose(world[5][0], [[-10, 0, 10], [0, -10, 10]], atol=1e-12)
        self.assertEqual((bones, tracks), before)
        self.assertEqual(derived[2]['parent'], 0)
        self.assertEqual(derived[4]['parent'], 0)

    def test_nonuniform_parent_scale(self):
        bones = fixture()
        bones[0]['scale'] = [2, 3, 4]
        bones[1]['rotation'] = [0, 0, math.sqrt(.5), math.sqrt(.5)]
        derived, _, error = derive(bones)
        # FTransform scales local X by 2 before rotating it onto world Y.
        np.testing.assert_allclose(transforms(derived)[3][0], [[0, 20, -260]], atol=1e-12)
        self.assertLess(max(error), 1e-10)

    def test_wrong_source_hierarchy_fails(self):
        bones = fixture()
        bones[2]['parent'] = 0
        with self.assertRaisesRegex(ValueError, 'spine-parented'):
            derive(bones)

    def test_negative_scale_fails(self):
        bones = fixture()
        bones[1]['scale'] = [-1, 1, 1]
        with self.assertRaisesRegex(ValueError, 'positive scale'):
            derive(bones)

    def test_missing_samples_fail(self):
        with self.assertRaisesRegex(ValueError, 'sample clock'):
            derive(fixture(), [dict(name='Bip001-Spine', keys=[])], 2)

    def test_unknown_or_duplicate_track_fails(self):
        bones = fixture()
        for tracks in ([dict(name='Absent', keys=[bones[0]])],
                       [dict(name='Bip001-Spine', keys=[bones[1]])] * 2):
            with self.assertRaisesRegex(ValueError, 'Duplicate or unknown'):
                derive(bones, tracks)


if __name__ == '__main__':
    unittest.main()

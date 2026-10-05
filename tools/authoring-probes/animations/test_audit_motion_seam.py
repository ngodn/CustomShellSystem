import copy
import unittest

from audit_motion_seam import audit


def frame(x=0, w=1):
    return {'pose': {'Snapshot': {
        'bIsValid': True, 'SkeletalMeshName': 'fixture', 'BoneNames': ['root'],
        'LocalTransforms': [{
            'Translation': dict(X=x, Y=0, Z=0),
            'Rotation': dict(X=0, Y=0, Z=0, W=w),
            'Scale3D': dict(X=1, Y=1, Z=1),
        }],
    }}}


class SeamAuditTests(unittest.TestCase):
    def test_equal_pose_and_opposite_quaternion_sign_can_have_a_velocity_seam(self):
        motion = {'fps': 30, 'frames': [frame(), frame(1), frame(w=-1)]}
        original = copy.deepcopy(motion)
        result = audit(motion)['bones'][0]
        self.assertEqual(result['position_gap_cm'], 0)
        self.assertEqual(result['orientation_gap_degrees'], 0)
        self.assertEqual(result['translation_velocity_gap_cm_s'], 60)
        self.assertEqual(motion, original)

    def test_static_motion_has_no_seam(self):
        result = audit({'fps': 60, 'frames': [frame(), frame(), frame()]})
        self.assertAlmostEqual(result['duration_seconds'], 2 / 60)
        self.assertEqual(result['bones'][0]['translation_velocity_gap_cm_s'], 0)

    def test_rejects_invalid_endpoints(self):
        for final in (frame(float('nan')), frame(w=0)):
            with self.subTest(final=final), self.assertRaises(ValueError):
                audit({'fps': 30, 'frames': [frame(), frame(), final]})
        for key, value in (('BoneNames', ['other']), ('bIsValid', False),
                           ('SkeletalMeshName', 'other'), ('LocalTransforms', [])):
            final = frame()
            final['pose']['Snapshot'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                audit({'fps': 30, 'frames': [frame(), frame(), final]})

    def test_rejects_static_photo_timeline_and_invalid_rate(self):
        for fps, frames in ((30, [frame()]), (0, [frame()] * 3),
                            (float('inf'), [frame()] * 3)):
            with self.subTest(fps=fps), self.assertRaises(ValueError):
                audit({'fps': fps, 'frames': frames})


if __name__ == '__main__':
    unittest.main()

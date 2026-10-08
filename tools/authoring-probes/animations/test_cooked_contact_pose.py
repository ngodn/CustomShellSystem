"""Exercise mesh translation retargeting independently of the capture parser."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

import numpy as np

from cooked_contact_pose import load_cooked, retarget_local, world_transforms


class RetargetContactTests(unittest.TestCase):
    def setUp(self):
        self.names = ['root', 'pelvis', 'calf']
        self.bones = [dict(name=n, parent=i-1, translation=[0, 0, i * 2],
                           rotation=[0, 0, 0, 1], scale=[1, 1, 1])
                      for i, n in enumerate(self.names)]
        self.skeleton = dict(ReferenceSkeleton=dict(FinalRefBoneInfo=[
            dict(Name=n, ParentIndex=i-1) for i, n in enumerate(self.names)]),
            Properties=dict(BoneTree=[dict(TranslationRetargetingMode='EBoneTranslationRetargetingMode::'+m)
                                     for m in ('Animation', 'AnimationScaled', 'Skeleton')]))
        self.reference = [dict(Translation=dict(X=0, Y=0, Z=1)) for _ in self.names]
        self.values = np.array([[[0., 0, 0, 1, 3, 4, 1, 1, 1, 1]] * 3] * 2)

    def evaluate(self, values=None):
        return retarget_local(self.values if values is None else values,
                              self.names, self.bones, self.skeleton, self.reference)

    def test_modes_keep_rotation_scale_and_animation_translation(self):
        original = self.values.copy()
        result = self.evaluate()
        np.testing.assert_array_equal(result[:, 0], original[:, 0])
        np.testing.assert_array_equal(result[:, 1, 4:7], original[:, 1, 4:7] * 2)
        np.testing.assert_array_equal(result[:, 2, 4:7], [[0, 0, 4]] * 2)
        np.testing.assert_array_equal(result[:, :, :4], original[:, :, :4])
        np.testing.assert_array_equal(result[:, :, 7:], original[:, :, 7:])
        np.testing.assert_array_equal(self.values, original)

    def test_zero_source_length_is_not_scaled(self):
        self.reference[1]['Translation']['Z'] = 0
        np.testing.assert_array_equal(self.evaluate()[:, 1], self.values[:, 1])

    def test_hierarchy_mismatch_is_rejected(self):
        self.bones[2]['parent'] = 0
        with self.assertRaisesRegex(ValueError, 'remapping'):
            self.evaluate()

    def test_unknown_mode_is_not_silently_ignored(self):
        self.skeleton['Properties']['BoneTree'][1]['TranslationRetargetingMode'] = 'OrientAndScale'
        with self.assertRaisesRegex(ValueError, 'Unimplemented'):
            self.evaluate()

    def test_unmapped_bone_is_rejected(self):
        self.names[2] = 'different'
        with self.assertRaisesRegex(ValueError, 'coverage'):
            self.evaluate()

    def test_nonfinite_and_nonunit_transforms_are_rejected(self):
        for component, value in ((0, float('nan')), (3, 2), (7, 0)):
            values = self.values.copy()
            values[0, 1, component] = value
            with self.assertRaisesRegex(ValueError, 'Invalid'):
                self.evaluate(values)

    def test_parent_rotation_and_scale_affect_child_world(self):
        local = self.values[:1, :2].copy()
        local[0, 0] = [0, 0, 2**-.5, 2**-.5, 1, 0, 0, 2, 2, 2]
        local[0, 1] = [0, 0, 0, 1, 1, 0, 0, 1, 1, 1]
        p, q, s = world_transforms(local, self.bones[:2])[1]
        np.testing.assert_allclose(p, [[1, 2, 0]], atol=1e-12)
        np.testing.assert_allclose(q, [local[0, 0, :4]], atol=1e-12)
        np.testing.assert_array_equal(s, [[2, 2, 2]])

    def test_cooked_capture_ignores_signed_scale_on_unused_accessory(self):
        names = self.names + ['unused_rope']
        values = np.concatenate((self.values, self.values[:, :1]), axis=1).astype('<f4')
        values[:, -1, 7:] = -1
        skeleton = json.loads(json.dumps(self.skeleton))
        skeleton['Type'] = 'Skeleton'
        skeleton['ReferenceSkeleton']['FinalRefBoneInfo'].append(dict(Name=names[-1], ParentIndex=0))
        skeleton['Properties']['BoneTree'].append(dict(TranslationRetargetingMode='Animation'))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / 'absolute-tracks.bin'
            binary.write_bytes(values.tobytes())
            manifest = dict(schema=1, layout='frame,track,component', componentType='float32-le',
                file=binary.name, roundingPolicy='none',
                components=['qx', 'qy', 'qz', 'qw', 'tx', 'ty', 'tz', 'sx', 'sy', 'sz'],
                frames=2, names=names, bytes=binary.stat().st_size,
                sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), sampleRate=30, asset='/Test')
            frames = []
            for index, frame in enumerate(values):
                transforms = [dict(Rotation=dict(zip('XYZW', map(float, t[:4]))),
                    Translation=dict(zip('XYZ', map(float, t[4:7]))),
                    Scale3D=dict(zip('XYZ', map(float, t[7:])))) for t in frame]
                frames.append(dict(frame=index, BoneNames=names, LocalTransforms=transforms))
            sample = dict(schema=2, asset='/Test', trackCount=4, wrapOptimized=False,
                logicalSamples=2, sampleRate=30, frames=frames,
                referencePose=self.reference + [dict(Translation=dict(X=0, Y=0, Z=1))])
            path = root / 'absolute-tracks-full.json'
            path.write_text(json.dumps(manifest))
            (root / 'absolute-tracks.json').write_text(json.dumps(sample))
            skeleton_path = root / 'skeleton.json'
            skeleton_path.write_text(json.dumps({'/Skeleton': [skeleton]}))
            actual, _ = load_cooked(path, self.bones, skeleton_path, {})
            expected = world_transforms(self.evaluate(), self.bones)
            for actual_bone, expected_bone in zip(actual, expected):
                for a, b in zip(actual_bone, expected_bone):
                    np.testing.assert_allclose(a, b)
            rope = dict(name=names[-1], parent=0, translation=[0, 0, 0],
                        rotation=[0, 0, 0, 1], scale=[1, 1, 1])
            with self.assertRaisesRegex(ValueError, 'Invalid retargeted transform'):
                load_cooked(path, self.bones + [rope], skeleton_path, {})


if __name__ == '__main__':
    unittest.main()

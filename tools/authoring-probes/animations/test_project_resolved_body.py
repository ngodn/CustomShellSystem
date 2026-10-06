import copy
import unittest

from project_resolved_body import project


class ProjectionTests(unittest.TestCase):
    def setUp(self):
        pose = dict(Rotation=dict(X=0, Y=0, Z=0, W=1),
                    Translation=dict(X=0, Y=0, Z=0), Scale3D=dict(X=1, Y=1, Z=1))
        self.skeleton = dict(Package='/Game/Rig', ReferenceSkeleton=dict(
            FinalRefBoneInfo=[dict(Name='Root', ParentIndex=-1), dict(Name='Hand', ParentIndex=0)],
            FinalRefBonePose=[pose, pose]))
        tracks = [dict(trackIndex=i, boneIndex=index, boneName=name,
                       rotations=[pose['Rotation']], rotationTimes=[],
                       translations=[pose['Translation']], translationTimes=[],
                       scales=[pose['Scale3D']], scaleTimes=[])
                  for i, (index, name) in enumerate(((0, 'Root'), (1, 'Hand'), (5, None)))]
        mapping = [dict(BoneTreeIndex=t['boneIndex']) for t in tracks]
        self.audit = dict(asset='/Game/Dance.Dance', skeleton='/Game/Rig.Rig',
                          skeletonBones=2, frames=31, duration=1, tracks=tracks)
        self.meta = dict(Package='/Game/Dance', Properties=dict(NumFrames=31, SequenceLength=1,
                         TrackToSkeletonMapTable=mapping), CompressedTrackToSkeletonMapTable=[0, 1, 5])

    def run_projection(self, required=('Root', 'Hand')):
        return project(self.audit, self.meta, self.skeleton, required)

    def test_unbound_tracks_preserved_without_fabricated_binding(self):
        before = copy.deepcopy((self.audit, self.meta, self.skeleton))
        result, retained = self.run_projection()
        self.assertTrue(result['auditionOnly'])
        self.assertEqual(result['unresolvedSourceBoneIndices'], [5])
        self.assertEqual(retained['tracks'], [self.audit['tracks'][2]])
        self.assertEqual(len(result['bones']) + len(retained['tracks']), 3)
        self.assertEqual(before, (self.audit, self.meta, self.skeleton))
        for projected, source in zip(result['bones'], self.audit['tracks']):
            self.assertEqual(projected['rotations'], source['rotations'])

    def test_omitted_required_ancestor_rejected(self):
        with self.assertRaisesRegex(ValueError, 'ancestors'):
            self.run_projection(('Hand',))

    def test_missing_required_joint_rejected(self):
        with self.assertRaisesRegex(ValueError, 'missing'):
            self.run_projection(('Root', 'Unknown'))

    def test_wrong_resolved_name_rejected(self):
        self.audit['tracks'][1]['boneName'] = 'Foot'
        with self.assertRaisesRegex(ValueError, 'bone name'):
            self.run_projection()

    def test_wrong_metadata_timeline_rejected(self):
        self.meta['Properties']['SequenceLength'] = 2
        with self.assertRaisesRegex(ValueError, 'timeline'):
            self.run_projection()

    def test_mismatched_compressed_map_rejected(self):
        self.meta['CompressedTrackToSkeletonMapTable'] = [0]
        with self.assertRaisesRegex(ValueError, 'Compressed track'):
            self.run_projection()

    def test_invalid_unbound_keys_also_rejected(self):
        self.audit['tracks'][2]['translationTimes'] = [float('nan')]
        with self.assertRaisesRegex(ValueError, 'key time'):
            self.run_projection()

    def test_additive_rejected(self):
        self.meta['Properties']['AdditiveAnimType'] = 'AAT_LocalSpaceBase'
        with self.assertRaisesRegex(ValueError, 'Additive'):
            self.run_projection()

    def test_invalid_reference_rejected(self):
        self.skeleton['ReferenceSkeleton']['FinalRefBonePose'][0]['Rotation']['W'] = 0
        with self.assertRaisesRegex(ValueError, 'quaternion'):
            self.run_projection()


if __name__ == '__main__':
    unittest.main()

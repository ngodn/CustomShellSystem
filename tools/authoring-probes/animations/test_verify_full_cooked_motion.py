import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np

import verify_full_cooked_motion as verify


class FullCookedMotionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.values = np.array([[[0, 0, 0, 1, i, 0, 0, 1, 1, 1]] for i in range(3)], dtype='<f4')
        transforms = [dict(Rotation=dict(zip('XYZW', [0, 0, 0, 1])),
                           Translation=dict(zip('XYZ', [i, 0, 0])), Scale3D=dict(zip('XYZ', [1, 1, 1])))
                      for i in range(3)]
        (self.root / 'raw.json').write_text(json.dumps(dict(fps=60, should_retarget=False, frames=[dict(pose=dict(Snapshot=dict(
            bIsValid=True, BoneNames=['root'], LocalTransforms=[t]))) for t in transforms])))
        (self.root / 'absolute-tracks.json').write_text(json.dumps(dict(schema=2, asset='trial',
            logicalSamples=3, sampleRate=60, trackCount=1, frames=[dict(frame=i,
                BoneNames=['root'], LocalTransforms=[transforms[i]]) for i in (0, 2)])))
        self.save_binary()

    def save_binary(self):
        data = self.values.tobytes()
        (self.root / 'absolute-tracks.bin').write_bytes(data)
        (self.root / 'absolute-tracks-full.json').write_text(json.dumps(dict(schema=1,
            asset='trial', frames=3, names=['root'], sampleRate=60, bytes=len(data),
            roundingPolicy='none',
            sha256=hashlib.sha256(data).hexdigest(), layout='frame,track,component',
            componentType='float32-le', file='absolute-tracks.bin',
            components=['qx', 'qy', 'qz', 'qw', 'tx', 'ty', 'tz', 'sx', 'sy', 'sz'])))

    def run_verify(self):
        with patch('sys.argv', ['verify', str(self.root), str(self.root / 'raw.json'),
                                str(self.root / 'result.json')]):
            verify.main()

    def test_all_keys_match(self):
        self.run_verify()
        self.assertTrue(json.loads((self.root / 'result.json').read_text())['passed'])

    def test_middle_error_is_caught_when_endpoint_samples_match(self):
        self.values[1, 0, 4] += .1
        self.save_binary()
        with self.assertRaisesRegex(ValueError, 'exceeds existing review limits'):
            self.run_verify()
        result = json.loads((self.root / 'result.json').read_text())
        self.assertFalse(result['passed'])
        self.assertEqual(result['worst']['position_cm'], dict(bone='root', frame=1))

    def test_changed_binary_is_rejected(self):
        path = self.root / 'absolute-tracks.bin'
        data = bytearray(path.read_bytes())
        data[40] ^= 1
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'checksum'):
            self.run_verify()
        self.assertFalse((self.root / 'result.json').exists())

    def test_oversized_dimensions_are_rejected_before_mapping_binary(self):
        path = self.root / 'absolute-tracks-full.json'
        data = json.loads(path.read_text())
        data.update(frames=100000, names=[f'bone_{i}' for i in range(41)])
        path.write_text(json.dumps(data))
        with patch.object(verify.np, 'memmap') as mapped:
            with self.assertRaisesRegex(ValueError, 'dimensions'):
                self.run_verify()
            mapped.assert_not_called()

    def test_unsampled_nonfinite_key_is_rejected(self):
        self.values[1, 0, 4] = np.nan
        self.save_binary()
        with self.assertRaisesRegex(ValueError, 'Non-finite'):
            self.run_verify()

    def test_binary_track_order_must_match_json_samples(self):
        path = self.root / 'absolute-tracks-full.json'
        data = json.loads(path.read_text())
        data['names'] = ['pelvis']
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'track order'):
            self.run_verify()

    def test_retargeted_reference_is_rejected(self):
        path = self.root / 'raw.json'
        data = json.loads(path.read_text())
        data['should_retarget'] = True
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'unretargeted'):
            self.run_verify()


if __name__ == '__main__':
    unittest.main()

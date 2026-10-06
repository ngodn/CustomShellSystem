"""The material scan must include knee-height vertices and exclude hidden surfaces."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import numpy as np


class MaterialContactTests(unittest.TestCase):
    def test_explicit_visible_surface(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dtype = np.dtype([('position', '<f4', 3), ('influences', [
                ('bone', '<u2'), ('raw', '<u2'), ('weight', '<f4')], 1)])
            vertices = np.zeros(6, dtype=dtype)
            vertices['position'] = [(0, 0, 50), (1, 0, 50), (0, 1, 50),
                                    (0, 0, 0), (1, 0, 0), (0, 1, 0)]
            vertices['influences']['raw'] = 65535
            vertices['influences']['weight'] = 1
            vertices.tofile(root / 'skin-vertices.bin')
            np.arange(6, dtype='<u4').tofile(root / 'skin-indices.bin')
            files = [dict(name=p.name, bytes=p.stat().st_size,
                          sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                     for p in sorted(root.glob('*.bin'))]
            audit = dict(schema=1, files=files, max_influences=1, vertex_stride=dtype.itemsize,
                vertex_count=6, index_count=6,
                bones=[dict(name='root', parent=-1, translation=[0, 0, 0],
                            rotation=[0, 0, 0, 1], scale=[1, 1, 1])],
                sections=[dict(MaterialIndex=0, FirstIndex=0, NumFaces=1),
                          dict(MaterialIndex=1, FirstIndex=3, NumFaces=1)])
            (root / 'skin-audit.json').write_text(json.dumps(audit))
            snapshot = dict(bIsValid=True, BoneNames=['root'], LocalTransforms=[dict(
                Translation=dict(X=0, Y=0, Z=-51), Rotation=dict(X=0, Y=0, Z=0, W=1),
                Scale3D=dict(X=1, Y=1, Z=1))])
            motion = root / 'motion.json'
            motion.write_text(json.dumps(dict(fps=60, frames=[dict(pose=dict(Snapshot=snapshot))])))
            for name, materials, success in [('visible', ['0'], True),
                                              ('missing', ['2'], False),
                                              ('duplicate', ['0', '0'], False)]:
                with self.subTest(name=name):
                    output = root / (name + '.json')
                    result = subprocess.run([sys.executable, str(Path(__file__).with_name('measure_skin_contact.py')),
                        str(root), str(output), str(motion), '--materials', *materials],
                        capture_output=True, text=True)
                    self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
                    self.assertEqual(output.exists(), success)
                    if success:
                        report = json.loads(output.read_text())
                        surface = report['results'][0]['sides']['surface']
                        self.assertEqual(surface['vertices'], 3)
                        self.assertAlmostEqual(surface['min_z_cm'], -1)
                        self.assertEqual(surface['frames_below_minus_0_1_cm'], 1)
                        self.assertEqual(surface['material_min_z_cm'], {'0': -1.0})


if __name__ == '__main__':
    unittest.main()

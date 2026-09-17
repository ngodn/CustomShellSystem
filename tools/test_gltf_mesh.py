"""Tests for gltf_mesh: reading, welding, and the vertex-order check.

The welding and clustering tests matter most. A glTF export splits vertices at
UV and normal seams, so the file never has the vertex count the morph library
expects, and a wrong weld would silently misindex every morph.

Run: python3 tools/test_gltf_mesh.py
"""
from __future__ import annotations

import base64
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import gltf_mesh


def make_glb(points, triangles, uvs=None) -> bytes:
    """Smallest valid glb holding one triangle primitive."""
    position = np.asarray(points, dtype=np.float32)
    index = np.asarray(triangles, dtype=np.uint32).reshape(-1)
    blobs = [position.tobytes(), index.tobytes()]
    accessors = [
        {'bufferView': 0, 'componentType': 5126, 'count': len(position), 'type': 'VEC3',
         'min': position.min(0).tolist(), 'max': position.max(0).tolist()},
        {'bufferView': 1, 'componentType': 5125, 'count': len(index), 'type': 'SCALAR'},
    ]
    attributes = {'POSITION': 0}
    if uvs is not None:
        uv = np.asarray(uvs, dtype=np.float32)
        blobs.append(uv.tobytes())
        accessors.append({'bufferView': 2, 'componentType': 5126,
                          'count': len(uv), 'type': 'VEC2'})
        attributes['TEXCOORD_0'] = 2

    views, offset = [], 0
    for blob in blobs:
        views.append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(blob)})
        offset += len(blob) + (-len(blob) % 4)
    binary = b''
    for blob in blobs:
        binary += blob + b'\x00' * (-len(blob) % 4)

    document = {
        'asset': {'version': '2.0'},
        'buffers': [{'byteLength': len(binary)}],
        'bufferViews': views,
        'accessors': accessors,
        'meshes': [{'name': 'Test', 'primitives': [
            {'attributes': attributes, 'indices': 1, 'mode': 4}]}],
    }
    js = json.dumps(document).encode()
    js += b' ' * (-len(js) % 4)
    total = 12 + 8 + len(js) + 8 + len(binary)
    out = b'glTF' + struct.pack('<II', 2, total)
    out += struct.pack('<I', len(js)) + b'JSON' + js
    out += struct.pack('<I', len(binary)) + b'BIN\x00' + binary
    return out


class TestReader(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp(prefix='glb-test-'))

    def write(self, name, *args, **kw):
        path = self.dir / name
        path.write_bytes(make_glb(*args, **kw))
        return path

    def test_reads_positions_and_triangles(self):
        path = self.write('tri.glb', [[0, 0, 0], [1, 0, 0], [0, 1, 0]], [[0, 1, 2]])
        mesh = gltf_mesh.load(path)
        self.assertEqual(mesh.vertex_count, 3)
        self.assertEqual(len(mesh.triangles), 1)
        np.testing.assert_allclose(mesh.points[1], [1, 0, 0])

    def test_reads_uvs(self):
        path = self.write('uv.glb', [[0, 0, 0], [1, 0, 0], [0, 1, 0]], [[0, 1, 2]],
                          uvs=[[0, 0], [1, 0], [0, 1]])
        mesh = gltf_mesh.load(path)
        self.assertIsNotNone(mesh.uvs)
        np.testing.assert_allclose(mesh.uvs[2], [0, 1], atol=1e-6)

    def test_missing_uvs_reported_as_none(self):
        path = self.write('nouv.glb', [[0, 0, 0], [1, 0, 0], [0, 1, 0]], [[0, 1, 2]])
        self.assertIsNone(gltf_mesh.load(path).uvs)

    def test_rejects_non_gltf(self):
        path = self.dir / 'junk.glb'
        path.write_bytes(b'not a gltf file at all')
        with self.assertRaises(Exception):
            gltf_mesh.load(path)

    def test_rejects_file_with_no_triangles(self):
        document = {'asset': {'version': '2.0'}, 'meshes': []}
        js = json.dumps(document).encode(); js += b' ' * (-len(js) % 4)
        blob = b'glTF' + struct.pack('<II', 2, 12 + 8 + len(js))
        blob += struct.pack('<I', len(js)) + b'JSON' + js
        path = self.dir / 'empty.glb'; path.write_bytes(blob)
        with self.assertRaises(gltf_mesh.GltfError):
            gltf_mesh.load(path)

    def test_data_uri_buffer(self):
        points = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float32)
        index = np.array([0, 1, 2], dtype=np.uint32)
        binary = points.tobytes() + index.tobytes()
        document = {
            'asset': {'version': '2.0'},
            'buffers': [{'byteLength': len(binary),
                         'uri': 'data:application/octet-stream;base64,'
                                + base64.b64encode(binary).decode()}],
            'bufferViews': [
                {'buffer': 0, 'byteOffset': 0, 'byteLength': points.nbytes},
                {'buffer': 0, 'byteOffset': points.nbytes, 'byteLength': index.nbytes}],
            'accessors': [
                {'bufferView': 0, 'componentType': 5126, 'count': 3, 'type': 'VEC3'},
                {'bufferView': 1, 'componentType': 5125, 'count': 3, 'type': 'SCALAR'}],
            'meshes': [{'primitives': [{'attributes': {'POSITION': 0}, 'indices': 1, 'mode': 4}]}],
        }
        path = self.dir / 'datauri.gltf'
        path.write_text(json.dumps(document))
        self.assertEqual(gltf_mesh.load(path).vertex_count, 3)


class TestWeld(unittest.TestCase):
    def test_merges_duplicate_positions(self):
        points = np.array([[0., 0, 0], [1., 0, 0], [0., 1, 0],
                           [1., 0, 0], [0., 1, 0], [1., 1, 0]])
        triangles = np.array([[0, 1, 2], [3, 4, 5]])
        welded, tris, mapping = gltf_mesh.weld(points, triangles)
        self.assertEqual(len(welded), 4)
        self.assertEqual(tris.max(), 3)
        self.assertEqual(mapping[1], mapping[3])
        self.assertEqual(mapping[2], mapping[4])

    def test_keeps_first_occurrence_order(self):
        points = np.array([[5., 0, 0], [0., 0, 0], [9., 0, 0], [0., 0, 0]])
        welded, _, mapping = gltf_mesh.weld(points, np.array([[0, 1, 2]]))
        np.testing.assert_allclose(welded[0], [5, 0, 0])
        np.testing.assert_allclose(welded[1], [0, 0, 0])
        np.testing.assert_allclose(welded[2], [9, 0, 0])
        self.assertEqual(mapping[3], 1)

    def test_nothing_to_merge_is_identity(self):
        points = np.array([[0., 0, 0], [1., 0, 0], [0., 1, 0]])
        triangles = np.array([[0, 1, 2]])
        welded, tris, mapping = gltf_mesh.weld(points, triangles)
        np.testing.assert_allclose(welded, points)
        np.testing.assert_array_equal(tris, triangles)
        np.testing.assert_array_equal(mapping, [0, 1, 2])

    def test_triangles_stay_valid(self):
        rng = np.random.default_rng(0)
        base = rng.normal(size=(30, 3))
        points = np.vstack([base, base[:10]])          # ten duplicates appended
        triangles = rng.integers(0, len(points), size=(40, 3))
        welded, tris, _ = gltf_mesh.weld(points, triangles)
        self.assertEqual(len(welded), 30)
        self.assertTrue((tris >= 0).all() and (tris < len(welded)).all())


class TestVertexOrderCheck(unittest.TestCase):
    def setUp(self):
        rng = np.random.default_rng(7)
        # A body-ish cloud: tall in z, narrow in x/y.
        self.points = rng.normal(size=(4000, 3)) * np.array([10.0, 10.0, 80.0])

    def test_real_region_is_clustered(self):
        centre = np.array([8.0, 0.0, 40.0])
        near = np.argsort(np.linalg.norm(self.points - centre, axis=1))[:180]
        result = gltf_mesh.morph_lands_where_expected(self.points, near)
        self.assertTrue(result['clustered'], result)
        self.assertLess(result['spread_ratio'], 0.5)

    def test_scattered_indices_are_rejected(self):
        rng = np.random.default_rng(11)
        scattered = rng.choice(len(self.points), 180, replace=False)
        result = gltf_mesh.morph_lands_where_expected(self.points, scattered)
        self.assertFalse(result['clustered'], result)
        self.assertGreater(result['spread_ratio'], 0.8)

    def test_separates_real_from_scattered_by_a_wide_margin(self):
        centre = np.array([0.0, 6.0, -30.0])
        near = np.argsort(np.linalg.norm(self.points - centre, axis=1))[:250]
        rng = np.random.default_rng(13)
        scattered = rng.choice(len(self.points), 250, replace=False)
        real = gltf_mesh.morph_lands_where_expected(self.points, near)['spread_ratio']
        noise = gltf_mesh.morph_lands_where_expected(self.points, scattered)['spread_ratio']
        self.assertLess(real * 2, noise, f'real {real:.3f} vs scattered {noise:.3f}')

    def test_rejects_out_of_range(self):
        with self.assertRaises(ValueError):
            gltf_mesh.morph_lands_where_expected(self.points, [0, 99999])

    def test_rejects_empty(self):
        with self.assertRaises(ValueError):
            gltf_mesh.morph_lands_where_expected(self.points, [])


class TestObjRoundTrip(unittest.TestCase):
    def test_write_and_reread(self):
        import build_figure_body
        directory = Path(tempfile.mkdtemp(prefix='obj-test-'))
        points = np.array([[0., 0, 0], [1., 0, 0], [0., 1, 0], [1., 1, 0]])
        triangles = np.array([[0, 1, 2], [1, 3, 2]])
        path = gltf_mesh.write_obj(directory / 'x.obj', points, triangles)
        back_points, back_tris = build_figure_body.load_target(path)
        np.testing.assert_allclose(back_points, points, atol=1e-6)
        np.testing.assert_array_equal(back_tris, triangles)


if __name__ == '__main__':
    unittest.main(verbosity=2)

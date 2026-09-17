"""Tests for conform_anatomy, mostly for `measure`.

`measure` is the number the whole anatomy effort is judged by, so it has to mean
what it claims: relief of a region's core above the skin around it, in
millimetres, positive when the middle stands proud.

Run: python3 tools/test_conform_anatomy.py
"""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import conform_anatomy as ca
import gltf_mesh
from test_shape_transfer import icosphere


def disc(radius=0.05, rings=14, spokes=24, bump=0.0, bump_width=0.3):
    """A flat disc in the z=0 plane with an optional gaussian bump at its centre.

    Stands in for a patch of skin with a nipple on it. Returns points, triangles.
    """
    points = [[0.0, 0.0, bump]]
    for ring in range(1, rings + 1):
        r = radius * ring / rings
        for spoke in range(spokes):
            angle = 2 * np.pi * spoke / spokes
            x, y = r * np.cos(angle), r * np.sin(angle)
            z = bump * np.exp(-((r / (radius * bump_width)) ** 2))
            points.append([x, y, z])
    triangles = []
    for spoke in range(spokes):
        nxt = (spoke + 1) % spokes
        triangles.append([0, 1 + spoke, 1 + nxt])
    for ring in range(rings - 1):
        base, above = 1 + ring * spokes, 1 + (ring + 1) * spokes
        for spoke in range(spokes):
            nxt = (spoke + 1) % spokes
            triangles.append([base + spoke, above + spoke, above + nxt])
            triangles.append([base + spoke, above + nxt, base + nxt])
    return np.asarray(points, dtype=np.float64), np.asarray(triangles, dtype=np.int64)


class TestMeasure(unittest.TestCase):
    def setUp(self):
        self.centre = np.zeros(3)
        self.normal = np.array([0.0, 0.0, 1.0])

    def test_flat_surface_has_no_relief(self):
        points, _ = disc(bump=0.0)
        result = ca.measure(points, self.centre, self.normal, 0.05)
        self.assertAlmostEqual(result['relief_mm'], 0.0, places=6)

    def test_bump_is_reported_in_millimetres(self):
        points, _ = disc(bump=0.008)          # 8 mm
        result = ca.measure(points, self.centre, self.normal, 0.05)
        self.assertAlmostEqual(result['relief_mm'], 8.0, delta=0.3)

    def test_dimple_reads_negative(self):
        points, _ = disc(bump=-0.004)
        result = ca.measure(points, self.centre, self.normal, 0.05)
        self.assertLess(result['relief_mm'], -3.0)

    def test_relief_scales_with_the_bump(self):
        small = ca.measure(disc(bump=0.003)[0], self.centre, self.normal, 0.05)['relief_mm']
        large = ca.measure(disc(bump=0.010)[0], self.centre, self.normal, 0.05)['relief_mm']
        self.assertGreater(large, small * 2.5)

    def test_counts_core_and_ring(self):
        points, _ = disc(bump=0.005)
        result = ca.measure(points, self.centre, self.normal, 0.05)
        self.assertGreater(result['core_vertices'], 0)
        self.assertGreater(result['ring_vertices'], 0)
        self.assertLessEqual(result['core_vertices'] + result['ring_vertices'],
                             result['vertices'])

    def test_empty_region_is_zero_not_an_error(self):
        points, _ = disc()
        result = ca.measure(points, np.array([10.0, 0, 0]), self.normal, 0.01)
        self.assertEqual(result['vertices'], 0)
        self.assertEqual(result['relief_mm'], 0.0)

    def test_relief_is_measured_along_the_given_normal(self):
        # Same disc, tilted: measuring along the tilted normal must give the same
        # answer as the flat case, or the metric is really just "height in z".
        points, _ = disc(bump=0.008)
        angle = np.radians(35)
        rotation = np.array([[1, 0, 0],
                             [0, np.cos(angle), -np.sin(angle)],
                             [0, np.sin(angle), np.cos(angle)]])
        tilted = points @ rotation.T
        result = ca.measure(tilted, self.centre, rotation @ self.normal, 0.05)
        self.assertAlmostEqual(result['relief_mm'], 8.0, delta=0.3)


class TestConform(unittest.TestCase):
    def test_conform_moves_a_flat_patch_onto_a_bumped_donor(self):
        target, target_tris = disc(bump=0.0)
        donor, donor_tris = disc(bump=0.008)
        marks = {'nipple_left': {'centre': np.zeros(3), 'normal': np.array([0.0, 0, 1])}}
        regions = {'nipple_left': {'radius': 0.05, 'strength': 1.0}}
        out, reports = ca.conform(target, target_tris, donor, donor_tris, marks, regions)
        row = reports['nipple_left']
        self.assertAlmostEqual(row['before']['relief_mm'], 0.0, places=5)
        self.assertGreater(row['after']['relief_mm'], 6.0)

    def test_conform_ignores_regions_with_no_landmark(self):
        target, target_tris = disc()
        donor, donor_tris = disc(bump=0.005)
        out, reports = ca.conform(target, target_tris, donor, donor_tris, marks={},
                                  regions=ca.REGIONS)
        self.assertEqual(reports, {})
        np.testing.assert_array_equal(out, target)


class TestLandmarks(unittest.TestCase):
    def test_reads_audit_fills(self):
        directory = Path(tempfile.mkdtemp(prefix='audit-'))
        path = directory / 'a.json'
        path.write_text(json.dumps({'fills': [
            {'label': 'nipple_left', 'center': [1, 2, 3], 'normal': [0, 0, 2]},
            {'label': 'elbow', 'center': [9, 9, 9], 'normal': [1, 0, 0]}]}))
        marks = ca.landmarks(path)
        self.assertIn('nipple_left', marks)
        self.assertNotIn('elbow', marks)
        np.testing.assert_allclose(marks['nipple_left']['normal'], [0, 0, 1])

    def test_normal_is_normalised(self):
        directory = Path(tempfile.mkdtemp(prefix='audit2-'))
        path = directory / 'a.json'
        path.write_text(json.dumps({'fills': [
            {'label': 'vulva', 'center': [0, 0, 0], 'normal': [3, 4, 0]}]}))
        marks = ca.landmarks(path)
        self.assertAlmostEqual(float(np.linalg.norm(marks['vulva']['normal'])), 1.0)


class TestObjIo(unittest.TestCase):
    def test_roundtrip_through_write_obj(self):
        directory = Path(tempfile.mkdtemp(prefix='objio-'))
        points, triangles = icosphere(1, radius=2.0)
        path = gltf_mesh.write_obj(directory / 'm.obj', points, triangles)
        back_points, back_tris = ca.load_obj(path)
        np.testing.assert_allclose(back_points, points, atol=1e-6)
        np.testing.assert_array_equal(back_tris, triangles)


if __name__ == '__main__':
    unittest.main(verbosity=2)

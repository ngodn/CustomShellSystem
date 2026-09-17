"""Tests for shape_transfer: alignment, binding and deformation carry-over.

Run: python3 tools/test_shape_transfer.py
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import shape_transfer as st


def icosphere(subdivisions=2, radius=1.0):
    """A sphere with triangle faces, for donors and targets of differing density."""
    phi = (1 + 5 ** .5) / 2
    verts = np.array([
        [-1, phi, 0], [1, phi, 0], [-1, -phi, 0], [1, -phi, 0],
        [0, -1, phi], [0, 1, phi], [0, -1, -phi], [0, 1, -phi],
        [phi, 0, -1], [phi, 0, 1], [-phi, 0, -1], [-phi, 0, 1]], dtype=float)
    faces = np.array([
        [0, 11, 5], [0, 5, 1], [0, 1, 7], [0, 7, 10], [0, 10, 11],
        [1, 5, 9], [5, 11, 4], [11, 10, 2], [10, 7, 6], [7, 1, 8],
        [3, 9, 4], [3, 4, 2], [3, 2, 6], [3, 6, 8], [3, 8, 9],
        [4, 9, 5], [2, 4, 11], [6, 2, 10], [8, 6, 7], [9, 8, 1]], dtype=np.int64)
    for _ in range(subdivisions):
        midpoint: dict[tuple[int, int], int] = {}
        new_faces = []
        verts = list(verts)
        def mid(i, j):
            key = (min(i, j), max(i, j))
            if key not in midpoint:
                verts.append((np.asarray(verts[i]) + np.asarray(verts[j])) / 2)
                midpoint[key] = len(verts) - 1
            return midpoint[key]
        for a, b, c in faces:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            new_faces += [[a, ab, ca], [b, bc, ab], [c, ca, bc], [ab, bc, ca]]
        verts = np.asarray(verts)
        faces = np.asarray(new_faces, dtype=np.int64)
    verts = verts / np.linalg.norm(verts, axis=1, keepdims=True) * radius
    return verts, faces


class TestSimilarityTransform(unittest.TestCase):
    def test_recovers_known_transform(self):
        rng = np.random.default_rng(0)
        source = rng.normal(size=(12, 3))
        angle = 0.7
        rotation = np.array([[np.cos(angle), -np.sin(angle), 0],
                             [np.sin(angle), np.cos(angle), 0],
                             [0, 0, 1]])
        scale, translation = 2.5, np.array([3.0, -1.0, 0.5])
        target = scale * source @ rotation.T + translation
        r, t, s = st.similarity_transform(source, target)
        self.assertAlmostEqual(s, scale, places=6)
        np.testing.assert_allclose(r, rotation, atol=1e-6)
        np.testing.assert_allclose(t, translation, atol=1e-6)

    def test_maps_points_onto_target(self):
        rng = np.random.default_rng(1)
        source = rng.normal(size=(20, 3))
        target = 1.7 * source + np.array([1.0, 2.0, 3.0])
        r, t, s = st.similarity_transform(source, target)
        np.testing.assert_allclose(s * source @ r.T + t, target, atol=1e-6)

    def test_never_reflects(self):
        # A target that is a mirrored source must not be fitted by a reflection.
        rng = np.random.default_rng(2)
        source = rng.normal(size=(10, 3))
        mirrored = source * np.array([1, 1, -1])
        r, _, _ = st.similarity_transform(source, mirrored)
        self.assertGreater(np.linalg.det(r), 0)

    def test_rejects_too_few_landmarks(self):
        with self.assertRaises(ValueError):
            st.similarity_transform(np.zeros((2, 3)), np.zeros((2, 3)))

    def test_rejects_coincident_landmarks(self):
        with self.assertRaises(ValueError):
            st.similarity_transform(np.ones((5, 3)), np.random.default_rng(3).normal(size=(5, 3)))


class TestClosestPoint(unittest.TestCase):
    def test_point_above_face_projects_inside(self):
        a = np.array([[0., 0, 0]]); b = np.array([[1., 0, 0]]); c = np.array([[0., 1, 0]])
        query = np.array([[0.25, 0.25, 5.0]])
        point, bary = st._closest_on_triangles(query, a, b, c)
        np.testing.assert_allclose(point[0], [0.25, 0.25, 0.0], atol=1e-9)
        self.assertAlmostEqual(bary[0].sum(), 1.0, places=9)
        self.assertTrue((bary >= -1e-9).all())

    def test_point_beyond_edge_clamps_to_edge(self):
        a = np.array([[0., 0, 0]]); b = np.array([[1., 0, 0]]); c = np.array([[0., 1, 0]])
        query = np.array([[2.0, -1.0, 0.0]])
        point, bary = st._closest_on_triangles(query, a, b, c)
        self.assertTrue((bary >= -1e-9).all() and bary.sum() <= 1 + 1e-9)
        # closest point of that triangle to (2,-1,0) is the vertex b
        np.testing.assert_allclose(point[0], [1.0, 0.0, 0.0], atol=1e-9)

    def test_degenerate_triangle_stays_finite(self):
        a = np.array([[0., 0, 0]]); b = np.array([[0., 0, 0]]); c = np.array([[0., 0, 0]])
        point, bary = st._closest_on_triangles(np.array([[1., 1, 1]]), a, b, c)
        self.assertTrue(np.isfinite(point).all() and np.isfinite(bary).all())


class TestTransfer(unittest.TestCase):
    def setUp(self):
        # Donor and target are spheres of different tessellation, so topology
        # genuinely differs, as G2F differs from a game body.
        self.donor_points, self.donor_faces = icosphere(2, radius=10.0)
        self.target_points, _ = icosphere(3, radius=10.0)

    def test_identity_morph_moves_nothing(self):
        moved, report = st.transfer(self.target_points, self.donor_points,
                                    self.donor_points.copy(), self.donor_faces,
                                    max_distance=5.0)
        np.testing.assert_allclose(moved, self.target_points, atol=1e-6)
        self.assertEqual(report['moved_vertices'], 0)

    def test_uniform_expansion_carries_over(self):
        # Growing the donor by 20% should grow the coincident target by ~20%.
        morphed = self.donor_points * 1.2
        moved, report = st.transfer(self.target_points, self.donor_points, morphed,
                                    self.donor_faces, max_distance=8.0, falloff=1.0)
        radius = np.linalg.norm(moved, axis=1)
        self.assertGreater(report['moved_vertices'], 0)
        # Faceting of the coarse donor means this is close, not exact.
        self.assertTrue(np.all(radius > 11.0), f'min radius {radius.min():.3f}')
        self.assertTrue(np.all(radius < 12.6), f'max radius {radius.max():.3f}')

    def test_local_bump_stays_local(self):
        morphed = self.donor_points.copy()
        top = morphed[:, 2] > 8.0
        morphed[top] += np.array([0, 0, 3.0])
        moved, _ = st.transfer(self.target_points, self.donor_points, morphed,
                               self.donor_faces, max_distance=3.0, falloff=1.0)
        delta = np.linalg.norm(moved - self.target_points, axis=1)
        # The far side of the sphere must be untouched.
        bottom = self.target_points[:, 2] < -8.0
        self.assertLess(delta[bottom].max(), 1e-6)
        self.assertGreater(delta[self.target_points[:, 2] > 8.0].max(), 0.5)

    def test_distant_vertices_are_left_alone(self):
        far = np.array([[500.0, 500.0, 500.0]])
        points = np.vstack([self.target_points, far])
        morphed = self.donor_points * 1.3
        moved, report = st.transfer(points, self.donor_points, morphed,
                                    self.donor_faces, max_distance=4.0)
        np.testing.assert_allclose(moved[-1], far[0], atol=1e-9)
        self.assertGreaterEqual(report['beyond_max_distance'], 1)

    def test_binding_can_be_reused(self):
        binding = st.bind(self.target_points, self.donor_points, self.donor_faces, max_distance=6.0)
        a, _ = st.transfer(self.target_points, self.donor_points, self.donor_points * 1.1,
                           self.donor_faces, max_distance=6.0, binding=binding)
        b, _ = st.transfer(self.target_points, self.donor_points, self.donor_points * 1.1,
                           self.donor_faces, max_distance=6.0)
        np.testing.assert_allclose(a, b, atol=1e-9)

    def test_report_counts_are_consistent(self):
        _, report = st.transfer(self.target_points, self.donor_points,
                                self.donor_points * 1.05, self.donor_faces, max_distance=6.0)
        self.assertEqual(report['total_vertices'], len(self.target_points))
        self.assertLessEqual(report['moved_vertices'], report['total_vertices'])


class TestRegionCentroid(unittest.TestCase):
    def test_centroid_of_known_region(self):
        points = np.array([[0., 0, 0], [2., 0, 0], [0., 2, 0], [100., 100, 100]])
        np.testing.assert_allclose(st.region_centroid(points, [0, 1, 2]),
                                   [2 / 3, 2 / 3, 0], atol=1e-9)

    def test_rejects_out_of_range_index(self):
        with self.assertRaises(ValueError):
            st.region_centroid(np.zeros((3, 3)), [0, 5])

    def test_rejects_empty_region(self):
        with self.assertRaises(ValueError):
            st.region_centroid(np.zeros((3, 3)), [])



class TestConformRegion(unittest.TestCase):
    """Conforming one region onto a donor shape, which is how anatomy is fixed."""

    def setUp(self):
        self.target, self.target_tris = icosphere(4, radius=10.0)
        donor, self.donor_tris = icosphere(4, radius=10.0)
        self.apex = np.array([0.0, 0.0, 10.0])
        gap = np.linalg.norm(donor - self.apex, axis=1)
        bump = np.exp(-(gap / 1.6) ** 2) * 1.8
        self.donor = donor + donor / np.linalg.norm(donor, axis=1, keepdims=True) * bump[:, None]

    def near_apex(self, points, radius=0.8):
        return np.linalg.norm(self.target - self.apex, axis=1) < radius

    def test_normal_projection_reaches_the_peak(self):
        out, report = st.conform_region_along_normal(
            self.target, self.target_tris, self.donor, self.donor_tris,
            centre=self.apex, radius=3.0)
        peak = np.linalg.norm(out[self.near_apex(out)], axis=1).max()
        self.assertAlmostEqual(peak, 11.8, places=2)
        self.assertEqual(report['missed'], 0)

    def test_nearest_point_undershoots_a_bulge(self):
        # Documents why the normal form is preferred: from inside a bulge the
        # closest surface point is on its side, not its tip.
        out, _ = st.conform_region(self.target, self.donor, self.donor_tris,
                                   centre=self.apex, radius=3.0)
        peak = np.linalg.norm(out[self.near_apex(out)], axis=1).max()
        self.assertLess(peak, 11.8)

    def test_outside_the_region_is_untouched(self):
        for conform in (
            lambda: st.conform_region(self.target, self.donor, self.donor_tris,
                                      centre=self.apex, radius=3.0)[0],
            lambda: st.conform_region_along_normal(self.target, self.target_tris, self.donor,
                                                   self.donor_tris, centre=self.apex, radius=3.0)[0],
        ):
            out = conform()
            far = np.linalg.norm(self.target - self.apex, axis=1) > 6.0
            np.testing.assert_allclose(out[far], self.target[far], atol=1e-12)

    def test_strength_scales_the_result(self):
        half, _ = st.conform_region_along_normal(
            self.target, self.target_tris, self.donor, self.donor_tris,
            centre=self.apex, radius=3.0, strength=0.5)
        full, _ = st.conform_region_along_normal(
            self.target, self.target_tris, self.donor, self.donor_tris,
            centre=self.apex, radius=3.0, strength=1.0)
        mask = self.near_apex(half)
        self.assertLess(np.linalg.norm(half[mask], axis=1).max(),
                        np.linalg.norm(full[mask], axis=1).max())

    def test_empty_region_is_a_no_op(self):
        out, report = st.conform_region(self.target, self.donor, self.donor_tris,
                                        centre=np.array([500.0, 0, 0]), radius=1.0)
        np.testing.assert_array_equal(out, self.target)
        self.assertEqual(report['region_vertices'], 0)

    def test_rejects_bad_radius(self):
        with self.assertRaises(ValueError):
            st.conform_region(self.target, self.donor, self.donor_tris, self.apex, radius=0)

    def test_rejects_bad_feather(self):
        with self.assertRaises(ValueError):
            st.conform_region(self.target, self.donor, self.donor_tris, self.apex,
                              radius=1.0, feather=1.0)

    def test_select_region_matches_conform(self):
        picked = st.select_region(self.target, self.apex, 3.0)
        _, report = st.conform_region(self.target, self.donor, self.donor_tris,
                                      centre=self.apex, radius=3.0)
        self.assertEqual(len(picked), report['region_vertices'])


class TestVertexNormals(unittest.TestCase):
    def test_sphere_normals_point_outward(self):
        points, triangles = icosphere(2, radius=5.0)
        normals = st.vertex_normals(points, triangles)
        outward = (normals * (points / np.linalg.norm(points, axis=1, keepdims=True))).sum(1)
        self.assertGreater(outward.min(), 0.9)

    def test_unit_length(self):
        points, triangles = icosphere(2, radius=3.0)
        lengths = np.linalg.norm(st.vertex_normals(points, triangles), axis=1)
        np.testing.assert_allclose(lengths, 1.0, atol=1e-9)



if __name__ == '__main__':
    unittest.main(verbosity=2)

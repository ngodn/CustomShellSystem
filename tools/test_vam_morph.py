"""Tests for vam_morph and vam_figure.

The format tests run on synthetic files so they work anywhere. The library tests
run against the real packs when `reference/CSS-v1.0.0-plan` is present, and skip
otherwise, so this suite stays useful on a machine without the reference set.

Run: python3 tools/test_vam_morph.py
"""
from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import vam_figure
import vam_morph

REFERENCE = Path(__file__).resolve().parents[1] / 'reference/CSS-v1.0.0-plan'


def write_vmb(path: Path, records):
    blob = struct.pack('<i', len(records))
    for index, dx, dy, dz in records:
        blob += struct.pack('<ifff', index, dx, dy, dz)
    path.write_bytes(blob)


class TestVmbFormat(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp(prefix='vmb-test-'))

    def test_roundtrip(self):
        path = self.dir / 'Sample.vmb'
        write_vmb(path, [(3, .1, .2, .3), (7, -.4, 0, .5)])
        indices, deltas = vam_morph.read_vmb(path)
        self.assertEqual(indices, [3, 7])
        self.assertAlmostEqual(deltas[0][0], .1, places=6)
        self.assertAlmostEqual(deltas[1][2], .5, places=6)

    def test_empty_morph_is_valid(self):
        path = self.dir / 'Empty.vmb'
        write_vmb(path, [])
        indices, deltas = vam_morph.read_vmb(path)
        self.assertEqual(indices, [])
        self.assertEqual(deltas, [])

    def test_truncated_file_is_refused(self):
        path = self.dir / 'Short.vmb'
        write_vmb(path, [(1, 0, 0, 0), (2, 0, 0, 0)])
        path.write_bytes(path.read_bytes()[:-4])
        with self.assertRaises(ValueError):
            vam_morph.read_vmb(path)

    def test_bogus_count_is_refused(self):
        path = self.dir / 'Lying.vmb'
        path.write_bytes(struct.pack('<i', 99999))
        with self.assertRaises(ValueError):
            vam_morph.read_vmb(path)

    def test_vmi_delta_count_must_agree(self):
        stem = self.dir / 'Mismatch'
        write_vmb(stem.with_suffix('.vmb'), [(1, 0, 0, 0)])
        stem.with_suffix('.vmi').write_text(json.dumps({'displayName': 'x', 'numDeltas': '5'}))
        with self.assertRaises(ValueError):
            vam_morph.load(stem.with_suffix('.vmb'))

    def test_vmi_formulas_are_parsed(self):
        stem = self.dir / 'WithJoints'
        write_vmb(stem.with_suffix('.vmb'), [(1, 0, 0, 0)])
        stem.with_suffix('.vmi').write_text(json.dumps({
            'displayName': 'With joints', 'numDeltas': '1', 'min': '0', 'max': '2',
            'formulas': [{'targetType': 'BoneCenterY', 'target': 'hip', 'multiplier': '0.0114'}]}))
        morph = vam_morph.load(stem.with_suffix('.vmb'))
        self.assertEqual(morph.maximum, 2.0)
        self.assertEqual(len(morph.formulas), 1)
        self.assertEqual(morph.formulas[0].target, 'hip')
        self.assertAlmostEqual(morph.formulas[0].multiplier, 0.0114)


class TestMorphBehaviour(unittest.TestCase):
    def morph(self, records, **kw):
        indices = [r[0] for r in records]
        deltas = [(r[1], r[2], r[3]) for r in records]
        return vam_morph.Morph('m', 'm', indices, deltas, **kw)

    def test_apply_at_full_weight(self):
        m = self.morph([(1, 1.0, 0, 0)])
        out = m.apply([(0, 0, 0), (0, 0, 0), (0, 0, 0)], 1.0)
        self.assertAlmostEqual(out[1][0], 1.0)
        self.assertAlmostEqual(out[0][0], 0.0)

    def test_apply_scales_with_weight(self):
        m = self.morph([(0, 2.0, 0, 0)])
        self.assertAlmostEqual(m.apply([(0, 0, 0)], 0.5)[0][0], 1.0)
        self.assertAlmostEqual(m.apply([(0, 0, 0)], 0.0)[0][0], 0.0)

    def test_apply_refuses_short_mesh(self):
        m = self.morph([(50, 1, 0, 0)])
        with self.assertRaises(ValueError) as caught:
            m.apply([(0, 0, 0)] * 10)
        self.assertIn('51', str(caught.exception))

    def test_targets_base_boundary(self):
        self.assertTrue(self.morph([(vam_morph.G2F_BASE_VERTICES - 1, 0, 0, 0)]).targets_base())
        self.assertFalse(self.morph([(vam_morph.G2F_BASE_VERTICES, 0, 0, 0)]).targets_base())

    def test_to_centimetres(self):
        m = self.morph([(0, 0.01, 0, 0)])
        self.assertAlmostEqual(m.to_centimetres().deltas[0][0], 1.0)

    def test_mismatched_lengths_refused(self):
        with self.assertRaises(ValueError):
            vam_morph.Morph('m', 'm', [1, 2], [(0, 0, 0)])


class TestFigure(unittest.TestCase):
    def test_defaults_resolve(self):
        stack = vam_figure.resolve({})
        self.assertIn('MonsterShinkai_BodyBase - Body', stack)

    def test_axes_are_independent(self):
        slim = vam_figure.resolve({'breast': 'slim', 'glutes': 'slim', 'figure': 'game'})
        omg = vam_figure.resolve({'breast': 'omg', 'glutes': 'slim', 'figure': 'game'})
        self.assertNotEqual(slim, omg)
        # changing breast must not touch the glutes contribution
        self.assertEqual({k: v for k, v in slim.items() if 'Booty' in k or 'thicc' in k},
                         {k: v for k, v in omg.items() if 'Booty' in k or 'thicc' in k})

    def test_weights_add_when_axis_and_detail_share_a_morph(self):
        stack = vam_figure.resolve({'figure': 'game', 'breast': 'slim', 'glutes': 'slim'},
                                   {'nipples': 0.5})
        self.assertAlmostEqual(stack['MonsterShinkai_NippleSize - Body'], 0.5)

    def test_unknown_tier_is_refused(self):
        with self.assertRaises(KeyError):
            vam_figure.resolve({'breast': 'enormous'})

    def test_unknown_detail_is_refused(self):
        with self.assertRaises(KeyError):
            vam_figure.resolve({}, {'elbows': 1.0})

    def test_every_tier_id_is_unique_per_axis(self):
        for axis in vam_figure.AXES:
            ids = [t.id for t in axis.tiers]
            self.assertEqual(len(ids), len(set(ids)), f'{axis.id} has duplicate tier ids')

    def test_default_tier_exists(self):
        for axis in vam_figure.AXES:
            axis.tier(axis.default)


@unittest.skipUnless(REFERENCE.is_dir(), 'reference morph packs not present')
class TestAgainstRealLibrary(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = {}
        for pack in ('MonsterShinkai.BodyMorphs.4.var', 'Skynet.Thicc_Morphs.1.var',
                     'Skynet.Booty_Shapes.1.var'):
            path = REFERENCE / pack
            if path.exists():
                for m in vam_morph.load_package(path):
                    cls.library[m.name] = m

    def test_library_loaded(self):
        self.assertGreaterEqual(len(self.library), 18)

    def test_every_morph_declared_count_matches(self):
        # load() already enforces this; reaching here means all 18 agreed.
        for name, morph in self.library.items():
            self.assertGreater(len(morph), 0, name)

    def test_known_topology_split(self):
        graft = {n for n, m in self.library.items() if not m.targets_base()}
        self.assertIn('MonsterShinkai_BodyBase - Body', graft)
        self.assertIn('MonsterShinkai_NippleSize - Body', graft)
        self.assertNotIn('Busty Breasts 1', graft)

    def test_body_base_carries_joint_formulas(self):
        self.assertEqual(len(self.library['MonsterShinkai_BodyBase - Body'].formulas), 415)

    def test_every_tier_stack_resolves_to_real_morphs(self):
        missing = set()
        for figure in vam_figure.FIGURE.tiers:
            for breast in vam_figure.BREAST.tiers:
                for glutes in vam_figure.GLUTES.tiers:
                    stack = vam_figure.resolve(
                        {'figure': figure.id, 'breast': breast.id, 'glutes': glutes.id})
                    missing |= {n for n in stack if n not in self.library}
        self.assertEqual(missing, set(), f'tier stacks name morphs not in the library: {missing}')

    def test_strain_grows_with_tier(self):
        base = {'figure': 'game', 'glutes': 'slim'}
        slim = vam_figure.strain(vam_figure.resolve(base | {'breast': 'slim'}), self.library)
        omg = vam_figure.strain(vam_figure.resolve(base | {'breast': 'omg'}), self.library)
        self.assertGreater(omg['largest_cm'], slim['largest_cm'])

    def test_no_tier_overshoots_a_sane_limit(self):
        # 15 cm is already extreme for a body morph; past that a recipe is wrong.
        for figure in vam_figure.FIGURE.tiers:
            for breast in vam_figure.BREAST.tiers:
                for glutes in vam_figure.GLUTES.tiers:
                    sel = {'figure': figure.id, 'breast': breast.id, 'glutes': glutes.id}
                    result = vam_figure.strain(vam_figure.resolve(sel), self.library)
                    self.assertLess(result['largest_cm'], 15.0, f'{sel} strains {result}')


if __name__ == '__main__':
    unittest.main(verbosity=2)

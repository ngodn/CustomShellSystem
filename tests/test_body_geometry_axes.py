"""Recipe-declared body solver axes (6 to 8) and the schema 2 geometry packer."""
import copy
import json
from pathlib import Path
import random
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
sys.path.insert(0, str(ROOT/'tools/authoring/body-geometry'))
from css_controls import validate
import body_geometry_axes as axes_module
from body_geometry_axes import NAMES, contact_inputs, corner_weights, layout_inputs, recipe_axes
import build_body_runtime_geometry as builder

LIBRARY = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
SIX = ['PBMWaistWidth', 'PBMHipSize', 'PBMThighsTone', 'PBMGlutesSize', 'PBMBreastsSize', 'FBMBodyTone']
EIGHT = SIX + ['PBMBreastsImplants', 'PBMBreastsCleavage']


def shape(morph, axis=None):
    control = dict(id='shape_'+morph, name=morph, kind='shape', group='body', role='figure', morph=morph,
                   min=0, max=1, default=[0, 0, 0, 1])
    if axis is not None:
        control['solver_axis'] = axis
    return control


def recipe(morphs, extra=()):
    return dict(schema=1, controls=[shape(m, i) for i, m in enumerate(morphs)] + [shape(m) for m in extra])


def measured(morphs, seed=1):
    """Synthetic measured layout and ellipsoids with all 2^n corners."""
    rng = random.Random(seed)
    n = len(morphs)
    helpers, regions = [], []
    for bone in NAMES:
        quadratic = [[0.]*n for _ in range(n)]
        for i in range(n):
            for j in range(i, n):
                quadratic[i][j] = quadratic[j][i] = rng.uniform(0, .5)
        helpers.append(dict(bone=bone, offset_cm=[rng.uniform(-5, 5) for _ in range(3)],
            offset_deltas_cm=[[rng.uniform(-1, 1) for _ in range(3)] for _ in range(n)],
            moment_cm2=rng.uniform(100, 300), moment_linear_cm2=[rng.uniform(0, 5) for _ in range(n)],
            moment_quadratic_cm2=quadratic))
        reference = [[1., 0., 0., rng.uniform(-10, 10)], [0., 0., -1., rng.uniform(-10, 10)],
                     [0., 1., 0., rng.uniform(-10, 10)], [0., 0., 0., 1.]]
        regions.append(dict(name=bone, parent='spine_04', reference_parent_matrix=reference,
            shape_basis_cm=[[3., .1, 0.], [0., 2., .2], [.1, 0., 1.5]],
            morph_ellipsoids=[dict(mask=m, center_cm=[rng.uniform(-20, 20) for _ in range(3)],
                                   scale=rng.uniform(.8, 1.4)) for m in range(1 << n)]))
    return dict(morphs=list(morphs), helpers=helpers), dict(morphs=list(morphs), regions=regions)


class SolverAxesInRecipes(unittest.TestCase):
    def test_validator_takes_six_to_eight_numbered_axes(self):
        for morphs in (SIX, EIGHT, EIGHT[:7]):
            with self.subTest(axes=len(morphs)):
                validate(recipe(morphs, extra=['PBMNipplesSize']))
        validate(dict(schema=1, controls=[shape(m) for m in EIGHT]))  # no axes: unchanged
        bad = []
        bad.append(recipe(SIX[:5]))
        bad.append(recipe(EIGHT + ['PBMChestWidth']))
        gap = recipe(SIX); gap['controls'][5]['solver_axis'] = 6; bad.append(gap)
        twice = recipe(SIX); twice['controls'][5]['solver_axis'] = 0; bad.append(twice)
        flag = recipe(SIX); flag['controls'][1]['solver_axis'] = True; bad.append(flag)
        text = recipe(SIX); text['controls'][1]['solver_axis'] = '1'; bad.append(text)
        same = recipe(SIX); same['controls'][5]['morph'] = same['controls'][4]['morph']; bad.append(same)
        rig = recipe(SIX); rig['controls'].append(dict(id='chest-motion', name='Chest', kind='rig', solver='angular_body',
            regions=['brust001', 'brust002'], frequency=dict(min=.5, max=6, default=2),
            damping_ratio=dict(min=.1, max=2, default=.7), motion_amount=dict(min=0, max=5, default=1), solver_axis=6))
        bad.append(rig)
        for case in bad:
            with self.subTest(case=json.dumps([c.get('solver_axis') for c in case['controls']])), self.assertRaises(ValueError):
                validate(case)

    def test_builder_reads_axes_in_order_from_a_manifest(self):
        block = recipe(list(reversed(EIGHT)))
        self.assertEqual(recipe_axes(block), list(reversed(EIGHT)))
        manifest = dict(catalog=dict(outfits=[dict(id='eve', variants=[
            dict(id='a', customize=copy.deepcopy(block)), dict(id='b', customize=copy.deepcopy(block)),
            dict(id='head', customize=dict(schema=1, controls=[]))])]))
        self.assertEqual(recipe_axes(manifest), list(reversed(EIGHT)))
        manifest['catalog']['outfits'][0]['variants'][1]['customize'] = recipe(EIGHT)
        with self.assertRaises(ValueError):
            recipe_axes(manifest)
        with self.assertRaises(ValueError):
            recipe_axes(dict(schema=1, controls=[shape(m) for m in EIGHT]))


class GeometryPacker(unittest.TestCase):
    def check_model(self, morphs):
        layout, ellipsoids = measured(morphs)
        model = builder.pack(morphs, layout, ellipsoids)
        n = len(morphs)
        self.assertEqual((model['schema'], model['morphs'], model['corners']), (2, morphs, 1 << n))
        self.assertEqual(list(model)[:3], ['schema', 'morphs', 'corners'])
        for helper in model['helpers']:
            self.assertEqual(len(helper['contact_centers_cm']), 1 << n)
            self.assertEqual(len(helper['contact_scales']), 1 << n)
            self.assertEqual(len(helper['offset_deltas_cm']), n)
        cases = builder.fixtures(morphs, layout, ellipsoids)
        self.assertEqual(len(cases), (1 << n) + 64)
        # At a corner the interpolated contact equals that corner's measured value.
        for mask in (0, 1, (1 << n) - 1, 1 << (n - 1)):
            values = [float((mask >> i) & 1) for i in range(n)]
            self.assertEqual(sum(corner_weights(values)), 1.)
            centers = contact_inputs(ellipsoids, values)['ContactCenters']
            packed = model['helpers'][0]['contact_centers_cm'][mask]
            for a, b in zip(centers[0], packed):
                self.assertAlmostEqual(a, b, places=9)
        # The last axis moves the model: no silent truncation to six.
        low, high = [0.]*n, [0.]*(n - 1) + [1.]
        self.assertNotEqual(layout_inputs(layout, low)['Offsets'], layout_inputs(layout, high)['Offsets'])
        self.assertNotEqual(contact_inputs(ellipsoids, low), contact_inputs(ellipsoids, high))
        return model

    def test_six_and_eight_axes(self):
        six = self.check_model(SIX)
        eight = self.check_model(EIGHT)
        self.assertGreater(len(json.dumps(eight)), 3*len(json.dumps(six)))

    def test_identity_and_corner_mismatches_are_refused(self):
        layout, ellipsoids = measured(EIGHT)
        with self.assertRaises(ValueError):
            builder.pack(SIX, layout, ellipsoids)
        swapped = copy.deepcopy(layout); swapped['morphs'] = list(reversed(EIGHT))
        with self.assertRaises(ValueError):
            builder.pack(EIGHT, swapped, ellipsoids)
        short = copy.deepcopy(ellipsoids); short['regions'][2]['morph_ellipsoids'].pop()
        with self.assertRaises(ValueError):
            builder.pack(EIGHT, layout, short)
        rows = copy.deepcopy(layout); rows['helpers'][0]['moment_linear_cm2'].pop()
        with self.assertRaises(ValueError):
            builder.pack(EIGHT, rows, ellipsoids)
        with self.assertRaises(ValueError):
            layout_inputs(layout, [0.]*6)

    def test_command_line_writes_schema_two_and_enforces_the_string_bound(self):
        layout, ellipsoids = measured(EIGHT)
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            for name, data in (('recipe', recipe(EIGHT)), ('layout', layout), ('ellipsoids', ellipsoids)):
                (folder/f'{name}.json').write_text(json.dumps(data))
            argv = ['build', '--recipe', str(folder/'recipe.json'), '--layout', str(folder/'layout.json'),
                    '--ellipsoids', str(folder/'ellipsoids.json'), '--output', str(folder/'model.json'),
                    '--fixtures', str(folder/'fixtures.json')]
            with mock.patch.object(sys, 'argv', argv), mock.patch('builtins.print'):
                builder.main()
            model = json.loads((folder/'model.json').read_text())
            self.assertEqual((model['schema'], model['corners'], model['morphs']), (2, 256, EIGHT))
            self.assertEqual(len(json.loads((folder/'fixtures.json').read_text())['cases']), 320)
            with mock.patch.object(sys, 'argv', argv), self.assertRaises(FileExistsError):
                builder.main()
            argv[argv.index('--output')+1] = str(folder/'small.json')
            argv = argv[:-2]
            with mock.patch.object(sys, 'argv', argv), mock.patch.object(builder, 'STRING_BOUND', 1000), \
                    self.assertRaises(ValueError):
                builder.main()

    @unittest.skipUnless((LIBRARY/'tools/body_morph_layout.py').exists(), 'SeduXtress authoring library not present')
    def test_six_axis_evaluators_match_the_library(self):
        # Read-only: never leave a bytecode cache in the shared authoring library.
        sys.path.append(str(LIBRARY/'tools'))
        writes, sys.dont_write_bytecode = sys.dont_write_bytecode, True
        try:
            import body_morph_layout
            import body_contact_morph_inputs
        finally:
            sys.path.remove(str(LIBRARY/'tools'))
            sys.dont_write_bytecode = writes
        layout, ellipsoids = measured(SIX, seed=7)
        rng = random.Random(3)
        for _ in range(20):
            values = [rng.random() for _ in SIX]
            self.assertEqual(layout_inputs(layout, values), body_morph_layout.inputs(layout, values))
            self.assertEqual(contact_inputs(ellipsoids, values), body_contact_morph_inputs.inputs(ellipsoids, values))


if __name__ == '__main__':
    unittest.main()

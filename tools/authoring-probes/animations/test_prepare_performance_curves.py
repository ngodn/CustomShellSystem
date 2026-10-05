import copy
import unittest

from prepare_performance_curves import evaluate, prepare, native_performance


def fixture():
    keys = [dict(Time=t, Value=v, InterpMode='RCIM_Linear', TangentMode='RCTM_Auto',
                 TangentWeightMode='RCTWM_WeightedNone', ArriveTangent=0,
                 ArriveTangentWeight=0, LeaveTangent=0, LeaveTangentWeight=0)
            for t, v in ((0, -0.13211426), (0.75, 1.116883), (2, 0.3))]
    return dict(Type='AnimSequence', Package='/fixture', Properties=dict(SequenceLength=2),
                CompressedCurveNames=[dict(DisplayName='squint_left')],
                CompressedCurveData=dict(FloatCurves=[dict(
                    CurveName='squint_left', CurveTypeFlags=4, FloatCurve=dict(
                        Keys=keys, DefaultValue=3.4028235e38,
                        PreInfinityExtrap='RCCE_Constant', PostInfinityExtrap='RCCE_Constant'))]))


class PerformanceCurveTests(unittest.TestCase):
    def test_native_catalog_keeps_every_source_key_and_uses_declared_shape_casing(self):
        source = fixture()
        before = copy.deepcopy(source)
        result = native_performance(source, ['Squint_Left'], 'css.eve.comecloser')
        self.assertEqual(result, dict(clip='css.eve.comecloser', duration=2,
            curves=[dict(morph='Squint_Left', keys=[[0, -0.13211426], [.75, 1.116883], [2, .3]])]))
        self.assertEqual(source, before)

    def test_native_catalog_rejects_missing_ambiguous_or_unsupported_shapes(self):
        for names in ([], ['squint'], ['squint_left', 'Squint_Left'], ['squint_left', 'bad name']):
            with self.subTest(names=names), self.assertRaises(ValueError):
                native_performance(fixture(), names, 'css.clip')
        for clip in ('original', 'off', '.', '..', 'css clip', 'a' * 97, None):
            with self.subTest(clip=clip), self.assertRaises(ValueError):
                native_performance(fixture(), ['squint_left'], clip)

    def test_native_catalog_rejects_invalid_weights_and_key_limits(self):
        for weight in (4.01, True):
            source = fixture()
            source['CompressedCurveData']['FloatCurves'][0]['FloatCurve']['Keys'][0]['Value'] = weight
            with self.subTest(weight=weight), self.assertRaises(ValueError):
                native_performance(source, ['squint_left'], 'css.clip')
        source = fixture()
        data = source['CompressedCurveData']['FloatCurves'][0]['FloatCurve']
        key = data['Keys'][0]
        data['Keys'] = [dict(key, Time=i/10000.) for i in range(16385)]
        with self.assertRaises(ValueError):
            native_performance(source, ['squint_left'], 'css.clip')

    def test_evaluation_keeps_key_values_linear_intervals_and_constant_extrapolation(self):
        curve = prepare(fixture(), ['squint_left'])['curves'][0]
        for seconds, expected in ((-1, -0.13211426), (0, -0.13211426),
                                  (0.375, (-0.13211426+1.116883)/2),
                                  (0.75, 1.116883), (2, 0.3), (10, 0.3)):
            with self.subTest(seconds=seconds):
                self.assertAlmostEqual(evaluate(curve, seconds), expected)
        with self.assertRaises(ValueError):
            evaluate(curve, float('nan'))

    def test_preserves_signed_weights_above_one_and_original_key_times(self):
        source = fixture()
        original = copy.deepcopy(source)
        result = prepare(source, ['Squint_Left'])
        self.assertEqual(result['curves'], source['CompressedCurveData']['FloatCurves'])
        self.assertEqual(result['missing_shapes'], [])
        self.assertEqual(result['summary'][0]['minimum'], -0.13211426)
        self.assertEqual(result['summary'][0]['maximum'], 1.116883)
        self.assertEqual(source, original)
        result['curves'][0]['FloatCurve']['Keys'][0]['Value'] = 99
        self.assertEqual(source, original)

    def test_reports_missing_shapes_without_guessing_aliases(self):
        result = prepare(fixture(), ['squint', 'squint_right'])
        self.assertEqual(result['missing_shapes'], ['squint_left'])
        self.assertEqual(len(result['curves']), 1)

    def test_rejects_mismatched_duplicate_or_nonstring_names(self):
        for name in ('other', '', None):
            source = fixture()
            source['CompressedCurveNames'][0]['DisplayName'] = name
            with self.subTest(name=name), self.assertRaises(ValueError):
                prepare(source, [])
        source = fixture()
        source['CompressedCurveNames'].append(dict(DisplayName='SQUINT_LEFT'))
        source['CompressedCurveData']['FloatCurves'] *= 2
        with self.assertRaises(ValueError):
            prepare(source, [])

    def test_rejects_changes_to_interpolation_and_invalid_times(self):
        for field, value in (('Time', 0), ('Time', 2.1), ('Time', float('nan')),
                             ('Value', float('inf')), ('InterpMode', 'RCIM_Cubic'),
                             ('TangentWeightMode', 'RCTWM_WeightedBoth')):
            source = fixture()
            source['CompressedCurveData']['FloatCurves'][0]['FloatCurve']['Keys'][1][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                prepare(source, [])
        source = fixture()
        source['CompressedCurveData']['FloatCurves'][0]['FloatCurve']['PostInfinityExtrap'] = 'RCCE_Cycle'
        with self.assertRaises(ValueError):
            prepare(source, [])

    def test_accepts_body_only_clips_without_fabricating_curves(self):
        source = fixture()
        del source['CompressedCurveNames'], source['CompressedCurveData']
        result = prepare(source, [])
        self.assertEqual(result['curves'], [])
        self.assertEqual(result['missing_shapes'], [])


if __name__ == '__main__':
    unittest.main()

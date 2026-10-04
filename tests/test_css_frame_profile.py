import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from css_frame_profile import summarize


class FrameProfileReportTests(unittest.TestCase):
    def setUp(self):
        self.report = {
            'phases': ['recovery', 'inventory', 'maintenance', 'attachments',
                       'seals', 'walk', 'misc', 'reconcile', 'astral'],
            'stop_reason': 'duration',
            'rows': [{'engine_ms': 20., 'interval_ms': 20., 'core_ms': 1.,
                      'phase_ms': [0.] * 8 + [i / 100.], 'failed': False}
                     for i in range(30)],
        }

    def test_current_runtime_astral_column(self):
        result = summarize(self.report)
        self.assertEqual(result['frames'], 30)
        self.assertAlmostEqual(result['metrics']['astral_ms']['mean'], .145)
        self.assertEqual(result['metrics']['astral_ms']['p95'], .28)
        self.assertEqual(result['engine_tick_rate_hz'], 50)
        self.assertIsNone(result['cssx_loaded'])

    def test_old_cssx_reports_remain_readable(self):
        self.report['phases'] = ['recovery', 'cssx_tick', 'hud_prepare', 'cssx_render', 'inventory']
        self.report['cssx_loaded'] = True
        for row in self.report['rows']:
            row['phase_ms'] = [.1] * 5
        self.assertTrue(summarize(self.report)['cssx_loaded'])

    def test_invalid_reports_are_rejected(self):
        for kind in ('layout', 'columns', 'nan', 'failed', 'duration', 'enclosing'):
            with self.subTest(kind=kind):
                report = copy.deepcopy(self.report)
                if kind == 'layout':
                    report['phases'].reverse()
                elif kind == 'columns':
                    report['rows'][0]['phase_ms'].pop()
                elif kind == 'nan':
                    report['rows'][0]['phase_ms'][0] = float('nan')
                elif kind == 'failed':
                    report['rows'][0]['failed'] = True
                elif kind == 'duration':
                    report['rows'].pop()
                elif kind == 'enclosing':
                    report['rows'][0]['core_ms'] = -1
                with self.assertRaises(ValueError):
                    summarize(report)


if __name__ == '__main__':
    unittest.main()

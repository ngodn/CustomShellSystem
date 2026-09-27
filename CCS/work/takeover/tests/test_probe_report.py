import json
from pathlib import Path
import tempfile
import unittest

from test_tools import ROOT, module

reporter = module(ROOT / "work/takeover/probe_report.py", "probe_report")


class ProbeReportTests(unittest.TestCase):
    def report(self, records):
        with tempfile.TemporaryDirectory(dir=ROOT / "work/takeover") as temporary:
            path = Path(temporary) / "probe.jsonl"
            path.write_text("\n".join(json.dumps(dict(session=1, run=1, **r)) for r in records))
            return reporter.summarize(path)

    def capture(self):
        return [dict(event="start", schema_version=1, probe="loaded_combat_01"),
                dict(event="player", granted_abilities=1),
                dict(event="interface"), dict(event="interface"),
                dict(event="ability", index=0, ability={"montage": {"path": "/Game/Live"}}),
                dict(event="end", state="complete", abilities_read=1, abilities_expected=1, maximum_step_us=20)]

    def test_complete_and_actual_paths(self):
        report = self.report(self.capture())
        self.assertTrue(report["verified_complete"])
        self.assertEqual(report["montages"], ["/Game/Live"])

    def test_missing_end_is_incomplete(self):
        self.assertFalse(self.report(self.capture()[:-1])["verified_complete"])

    def test_terminal_claim_cannot_hide_missing_grants(self):
        records = self.capture()
        del records[4]
        self.assertFalse(self.report(records)["verified_complete"])

    def test_error_invalidates_completion(self):
        records = self.capture()
        records.insert(-1, dict(event="error", message="Player generation changed"))
        self.assertFalse(self.report(records)["verified_complete"])

    def test_latest_capture_supersedes_previous_success(self):
        records = self.capture() + [dict(event="start", schema_version=1, probe="loaded_combat_01")]
        self.assertFalse(self.report(records)["verified_complete"])

    def registry_capture(self):
        records = self.capture()
        records[0]["probe"] = "loaded_combat_02"
        path = "/Game/Live"
        positive = dict(event="registry_query", control="known_montage", passed=True, returned=True,
                        expected_path=path, assets=[dict(path=path, **{"class": "/Script/Engine.AnimMontage"})])
        negative = dict(event="registry_query", control="nonexistent_package", passed=True, returned=False, assets=[])
        folder = dict(event="registry_query", control="known_folder", passed=True, returned=True,
                      expected_path=path, assets=[dict(path=path)])
        return records[:-1] + [positive, negative, folder, records[-1]]

    def test_registry_requires_actual_controls(self):
        records = self.registry_capture()
        self.assertTrue(self.report(records)["verified_complete"])
        records[-3]["assets"] = [dict(path="/Game/Unexpected")]
        self.assertFalse(self.report(records)["verified_complete"])

    def test_registry_success_flag_cannot_hide_empty_positive(self):
        records = self.registry_capture()
        records[-4]["assets"] = []
        self.assertFalse(self.report(records)["verified_complete"])

    def test_registry_cannot_use_the_first_probe_completion(self):
        records = self.capture()
        records[0]["probe"] = "loaded_combat_02"
        self.assertFalse(self.report(records)["verified_complete"])

    def test_gap_rejected(self):
        records = self.capture()
        records[4]["index"] = 2
        with self.assertRaisesRegex(ValueError, "out of order"):
            self.report(records)

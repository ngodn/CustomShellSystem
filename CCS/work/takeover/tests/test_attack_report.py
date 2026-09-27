import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("ccs_attack_report", ROOT / "work/takeover/attack_report.py")
REPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPORT)


class AttackReportTests(unittest.TestCase):
    def rows(self):
        identity = {"session": 100, "run": 1}
        return [
            {**identity, "event": "start", "schema": 1, "probe": "attack_call_03", "read_only": True},
            {**identity, "event": "interface", "native": True, "parameters": 8, "frame_size": 56},
            {**identity, "event": "call", "number": 0, "rate": 1.0, "start_time": 0.0,
             "player_outer_match": True, "combat_verified": False,
             "ability": {"live": True, "path": "/Game/Test.Ability", "index": 1, "serial": 1001},
             "montage": {"live": True, "path": "/Game/Test.Montage", "index": 2, "serial": 1002}},
            {**identity, "event": "end", "state": "complete", "recorded": 1, "seen": 2, "skipped": 1,
             "failures": 0, "maximum_callback_us": 10, "hook_removed": True, "callsite_observed": True},
        ]

    def report(self, rows):
        with tempfile.TemporaryDirectory(dir=ROOT / "work/takeover") as directory:
            path = Path(directory) / "trace.jsonl"
            path.write_text("".join(json.dumps(row) + "\n" for row in rows))
            return REPORT.summarize(path)

    def test_complete_trace_is_observation_only(self):
        value = self.report(self.rows())
        self.assertTrue(value["capture_complete"])
        self.assertTrue(value["player_outer_calls_observed"])
        self.assertFalse(value["combat_verified"])

    def test_missing_terminal_is_incomplete(self):
        self.assertFalse(self.report(self.rows()[:-1])["capture_complete"])

    def test_zero_calls_cannot_prove_callsite(self):
        rows = self.rows(); del rows[2]
        rows[-1].update(recorded=0, seen=1)
        self.assertTrue(self.report(rows)["capture_complete"])
        self.assertFalse(self.report(rows)["player_outer_calls_observed"])

    def test_missing_cleanup_or_counter_failure(self):
        for change in ({"hook_removed": False}, {"recorded": 2}, {"failures": 1}, {"maximum_callback_us": 2001},
                       {"recorded": True}, {"identity_skipped": True}, {"identity_skipped": -1}, {"identity_skipped": 2}):
            rows = self.rows(); rows[-1].update(change)
            self.assertFalse(self.report(rows)["capture_complete"])

    def test_gap_rejected(self):
        rows = self.rows(); rows[2]["number"] = 1
        with self.assertRaises(ValueError): self.report(rows)

    def test_nonfinite_or_false_compatibility_rejected(self):
        for change in ({"rate": float("nan")}, {"combat_verified": True}, {"player_outer_match": False}):
            rows = self.rows(); rows[2].update(change)
            with self.assertRaises(ValueError): self.report(rows)

    def test_expired_reference_cannot_prove_callsite(self):
        rows = self.rows(); rows[2]["montage"]["live"] = False
        self.assertFalse(self.report(rows)["player_outer_calls_observed"])

    def test_uninitialized_or_invalid_identity_cannot_prove_callsite(self):
        for field in ("ability", "montage"):
            for change in ({"serial": 0}, {"serial": -1}, {"serial": True}, {"index": -1}, {"index": False}):
                rows = self.rows(); rows[2][field].update(change)
                self.assertFalse(self.report(rows)["player_outer_calls_observed"])
            rows = self.rows(); del rows[2][field]["serial"]
            self.assertFalse(self.report(rows)["player_outer_calls_observed"])

    def test_latest_capture_and_late_records(self):
        rows = self.rows(); second = copy.deepcopy(rows[:-1])
        for row in second: row["run"] = 2
        self.assertFalse(self.report(rows + second)["capture_complete"])
        with self.assertRaises(ValueError): self.report(rows + [rows[2]])

    def test_event_context_keeps_only_active_nonempty_tags(self):
        for tag, active, expected in (("Ability.Attack.Light", True, ["Ability.Attack.Light"]),
                                      ("Ability.Attack.Light", False, []), ("None", True, []), ("", True, [])):
            rows = self.rows(); rows[0]["schema"] = 2
            rows[1]["event_context_layout"] = self.event_layout()
            rows[2].update(current_event_tag=tag, ability_active=active)
            value = self.report(rows)
            self.assertTrue(value["player_outer_calls_observed"])
            self.assertEqual(value["active_event_tags"], expected)
            self.assertFalse(value["combat_verified"])

    def event_layout(self):
        return {"event_tag_offset": 232, "active_offset": 904,
                "event_type": "/Script/GameplayAbilities.GameplayEventData",
                "tag_type": "/Script/GameplayTags.GameplayTag"}

    def test_event_layout_required_for_completed_schema_two(self):
        for layout in (None, {}, {**self.event_layout(), "active_offset": True},
                       {**self.event_layout(), "event_type": "/Game/Other"},
                       {**self.event_layout(), "event_tag_offset": 65536}):
            rows = self.rows(); rows[0]["schema"] = 2
            rows[1]["event_context_layout"] = layout
            rows[2].update(current_event_tag="None", ability_active=True)
            self.assertFalse(self.report(rows)["capture_complete"])

    def test_event_context_requires_typed_bounded_fields(self):
        for change in ({}, {"current_event_tag": "Tag"}, {"current_event_tag": "Tag", "ability_active": 1},
                       {"current_event_tag": "x" * 1025, "ability_active": True},
                       {"current_event_tag": None, "ability_active": False}):
            rows = self.rows(); rows[0]["schema"] = 2; rows[2].update(change)
            with self.assertRaises(ValueError): self.report(rows)

    def test_schema_boolean_is_not_a_version(self):
        rows = self.rows(); rows[0]["schema"] = True
        with self.assertRaises(ValueError): self.report(rows)

    def transient_rows(self):
        rows = self.rows(); rows[0]["schema"] = 3
        rows[1].update(event_context_layout=self.event_layout(), observation_name_limit=8)
        rows[2].update(current_event_tag="None", ability_active=True)
        for field in ("ability", "montage"):
            rows[2][field].update(live_at_callback=True, retained_identity=True, ancestry_complete=True,
                                  observed_names=[field, "/Game/Test"])
        rows[2]["ability_class"] = {"live": False, "path": "", "live_at_callback": True,
            "retained_identity": False, "ancestry_complete": True, "index": 3, "serial": 0,
            "observed_names": ["Ability_C", "/Game/Test"]}
        rows[-1]["unretained_calls"] = 0
        return rows

    def test_transient_call_observation_is_not_a_retained_reference(self):
        rows = self.transient_rows()
        for field in ("ability", "montage"):
            rows[2][field].update(live=False, path="", serial=0, retained_identity=False)
        rows[-1]["unretained_calls"] = 1
        value = self.report(rows)
        self.assertTrue(value["capture_complete"])
        self.assertTrue(value["player_outer_calls_observed"])
        self.assertFalse(value["retained_live_references"])
        self.assertFalse(value["combat_verified"])

    def test_transient_schema_rejects_invented_lifetime_and_unbounded_names(self):
        for change in ({"serial": 0}, {"live_at_callback": False}, {"retained_identity": False},
                       {"observed_names": []}, {"observed_names": ["x"] * 9},
                       {"observed_names": ["x" * 1025]}, {"index": True}):
            rows = self.transient_rows(); rows[2]["ability"].update(change)
            with self.assertRaises(ValueError): self.report(rows)

    def test_transient_count_must_match_rows(self):
        for value in (None, True, -1, 1):
            rows = self.transient_rows(); rows[-1]["unretained_calls"] = value
            self.assertFalse(self.report(rows)["capture_complete"])


class AttackReportSchemaFourTests(AttackReportTests):
    def hooks(self, registered=False, matched=(1, 0, 0), seen=(2, 5, 40)):
        labels = ("task_factory", "ready_for_activation", "control_getter")
        return [{"label": label, "function": f"/Script/Test.{label}", "registered": registered, "seen": s,
                 "matched": m, "func_before": "0x1", "func_after": "0x2", "func_changed": True}
                for label, s, m in zip(labels, seen, matched)]

    def stats(self, **changes):
        return {"available": True, "slots": 0, "running": 0, "stopped": False, "calls": 47, "wrong_thread": 0,
                "failures": 0, **changes}

    def four_rows(self):
        rows = self.transient_rows(); rows[0]["schema"] = 4
        rows[1].update(hooks=self.hooks(registered=True, matched=(0, 0, 0), seen=(0, 0, 0)),
                       task_layout={"montage_offset": 216, "ability_offset": 104}, host_stats=self.stats(calls=0))
        rows[2]["source"] = "task_factory"
        rows[-1].update(seen=7, skipped=6, hooks=self.hooks(), control_seen=40, host_stats=self.stats(),
                        factory_observed=True, ready_observed=False)
        return rows

    def test_schema_four_complete_capture_reports_sources_and_counters(self):
        value = self.report(self.four_rows())
        self.assertTrue(value["capture_complete"])
        self.assertTrue(value["player_outer_calls_observed"])
        self.assertEqual(value["sources"], {"task_factory": 1, "ready_for_activation": 0})
        self.assertEqual(value["control_seen"], 40)
        self.assertEqual(value["host_stats"]["calls"], 47)
        self.assertEqual(value["func_swapped"], {label: True for label in ("task_factory", "ready_for_activation", "control_getter")})
        self.assertFalse(value["combat_verified"])

    def test_schema_four_requires_source_label(self):
        for source in (None, "", "other"):
            rows = self.four_rows(); rows[2]["source"] = source
            with self.assertRaises(ValueError): self.report(rows)

    def test_schema_four_mismatched_hook_counts_rejected(self):
        rows = self.four_rows(); rows[-1]["hooks"] = self.hooks(matched=(0, 1, 0))
        with self.assertRaises(ValueError): self.report(rows)

    def test_schema_four_incomplete_without_hooks_stats_or_removal(self):
        for change in ({"hooks": None}, {"hooks": self.hooks(registered=True)}, {"host_stats": None},
                       {"host_stats": self.stats(available=False)}, {"control_seen": -1}, {"control_seen": True},
                       {"hooks": self.hooks(matched=(2, 0, 0))}):
            rows = self.four_rows(); rows[-1].update(change)
            if change.get("hooks") and change["hooks"][0]["matched"] == 2:
                with self.assertRaises(ValueError): self.report(rows)
                continue
            self.assertFalse(self.report(rows)["capture_complete"])

    def test_schema_four_zero_calls_is_complete_but_unobserved(self):
        rows = self.four_rows(); del rows[2]
        rows[-1].update(recorded=0, seen=6, unretained_calls=0, hooks=self.hooks(matched=(0, 0, 0)), factory_observed=False)
        value = self.report(rows)
        self.assertTrue(value["capture_complete"])
        self.assertFalse(value["player_outer_calls_observed"])

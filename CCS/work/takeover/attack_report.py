#!/usr/bin/env python3
"""Review a bounded native-task trace without treating missing calls as success."""
import argparse
import json
import math
from pathlib import Path


def validate_observation(value: dict, required: bool) -> None:
    if not isinstance(value, dict) or any(type(value.get(key)) is not bool for key in
            ("live", "live_at_callback", "retained_identity", "ancestry_complete")):
        raise ValueError("Invalid callback object observation")
    names = value.get("observed_names")
    if not isinstance(names, list) or len(names) > 8 or any(not isinstance(name, str) or not name or
            len(name.encode("utf-8")) > 1024 for name in names):
        raise ValueError("Invalid observed name chain")
    if type(value.get("index")) is not int or type(value.get("serial")) is not int or not isinstance(value.get("path"), str):
        raise ValueError("Invalid observed object identity")
    if value["live_at_callback"]:
        if value["index"] < 0 or value["serial"] < 0 or not names:
            raise ValueError("Missing callback object facts")
    elif required or value["index"] != -1 or value["serial"] != 0 or names or value["retained_identity"]:
        raise ValueError("Invalid absent callback object")
    if value["retained_identity"] and value["serial"] <= 0:
        raise ValueError("Uninitialized retained object identity")
    if value["live"]:
        if not value["retained_identity"] or not value["path"]:
            raise ValueError("Trace invents a live retained reference")
    elif value["path"]:
        raise ValueError("Trace invents a resolved object path")


HOOK_LABELS = ("task_factory", "ready_for_activation", "control_getter")


def validate_hooks(value) -> list | None:
    """Return the hook list when it carries three well-typed entries, else None."""
    if not isinstance(value, list) or [hook.get("label") for hook in value if isinstance(hook, dict)] != list(HOOK_LABELS):
        return None
    for hook in value:
        if any(type(hook.get(field)) is not int or hook[field] < 0 for field in ("seen", "matched")):
            return None
        if type(hook.get("registered")) is not bool or type(hook.get("func_changed")) is not bool:
            return None
        if not isinstance(hook.get("function"), str) or len(hook["function"]) > 1024:
            return None
    return value


def summarize(path: Path) -> dict:
    key = None
    start = interface = end = None
    calls, errors = [], []
    with path.open("rb") as stream:
        for number, line in enumerate(iter(lambda: stream.readline(65537), b""), 1):
            if len(line) > 65536:
                raise ValueError(f"Oversized record at line {number}")
            if not line.strip():
                continue
            row = json.loads(line)
            if not isinstance(row, dict):
                raise ValueError("Trace record is not an object")
            if row.get("event") == "start":
                if type(row.get("schema")) is not int or row["schema"] not in (1, 2, 3, 4) or row.get("probe") != "attack_call_03" or row.get("read_only") is not True:
                    raise ValueError("Unsupported attack trace schema")
                if any(type(row.get(field)) is not int or row[field] <= 0 for field in ("session", "run")):
                    raise ValueError("Invalid capture identity")
                start, interface, end, calls, errors = row, None, None, [], []
                key = (row["session"], row["run"])
                continue
            if key is None or (row.get("session"), row.get("run")) != key:
                continue
            if end is not None:
                raise ValueError("Records found after capture end")
            event = row.get("event")
            if event == "interface":
                if interface is not None:
                    raise ValueError("Duplicate attack interface")
                interface = row
            elif event == "call":
                if type(row.get("number")) is not int or row["number"] != len(calls) or len(calls) >= 64:
                    raise ValueError("Attack call records are missing or exceed the bound")
                for field in ("rate", "start_time"):
                    value = row.get(field)
                    if type(value) not in (int, float) or not math.isfinite(value):
                        raise ValueError("Invalid task input")
                if row.get("player_outer_match") is not True or row.get("combat_verified") is not False:
                    raise ValueError("Trace misrepresents ownership or compatibility")
                if start["schema"] >= 2:
                    tag = row.get("current_event_tag")
                    if not isinstance(tag, str) or len(tag) > 1024 or type(row.get("ability_active")) is not bool:
                        raise ValueError("Invalid ability event context")
                if start["schema"] >= 3:
                    for field in ("ability", "ability_class", "montage"):
                        validate_observation(row.get(field), field != "montage")
                if start["schema"] >= 4 and row.get("source") not in ("task_factory", "ready_for_activation"):
                    raise ValueError("Attack call source is missing or unknown")
                calls.append(row)
            elif event == "error":
                errors.append(row.get("message"))
            elif event == "end":
                end = row
            else:
                raise ValueError("Unknown trace event")
    if start is None:
        raise ValueError("No attack capture found")
    terminal = end or {}
    native = bool(interface and interface.get("native") is True and interface.get("parameters") == 8 and
                  type(interface.get("frame_size")) is int and 0 < interface["frame_size"] <= 512)
    if start["schema"] >= 2:
        layout = (interface or {}).get("event_context_layout")
        native = bool(native and isinstance(layout, dict) and
                      all(type(layout.get(field)) is int and 0 <= layout[field] < 65536
                          for field in ("event_tag_offset", "active_offset")) and
                      layout.get("event_type") == "/Script/GameplayAbilities.GameplayEventData" and
                      layout.get("tag_type") == "/Script/GameplayTags.GameplayTag")
    if start["schema"] >= 3:
        native = bool(native and type((interface or {}).get("observation_name_limit")) is int and
                      interface["observation_name_limit"] == 8)
    hooks = None
    if start["schema"] >= 4:
        hooks = validate_hooks((interface or {}).get("hooks"))
        native = bool(native and hooks is not None and isinstance((interface or {}).get("task_layout"), dict))
    counters = all(type(terminal.get(field)) is int and terminal[field] >= 0
                   for field in ("recorded", "seen", "skipped", "failures", "maximum_callback_us"))
    if "identity_skipped" in terminal:
        counters = bool(counters and type(terminal["identity_skipped"]) is int and
                        0 <= terminal["identity_skipped"] <= terminal["skipped"])
    if start["schema"] >= 3:
        expected_unretained = sum(not row["ability"]["retained_identity"] or
                                  not row["montage"]["retained_identity"] for row in calls)
        counters = bool(counters and type(terminal.get("unretained_calls")) is int and
                        terminal["unretained_calls"] == expected_unretained)
    terminal_hooks = None
    if start["schema"] >= 4:
        terminal_hooks = validate_hooks(terminal.get("hooks"))
        stats = terminal.get("host_stats")
        counters = bool(counters and terminal_hooks is not None and type(terminal.get("control_seen")) is int and
                        terminal["control_seen"] >= 0 and isinstance(stats, dict) and stats.get("available") is True and
                        all(type(stats.get(field)) is int and stats[field] >= 0
                            for field in ("slots", "running", "calls", "wrong_thread", "failures")) and
                        sum(hook["matched"] for hook in terminal_hooks) == terminal["recorded"] and
                        all(hook["registered"] is False for hook in terminal_hooks))
    complete = bool(native and counters and terminal.get("state") == "complete" and not errors and
                    terminal["recorded"] == len(calls) and terminal["seen"] == len(calls) + terminal["skipped"] and
                    terminal["failures"] == 0 and terminal["maximum_callback_us"] <= 2000 and
                    terminal.get("hook_removed") is True)
    live_references = all(isinstance(row.get(field), dict) and row[field].get("live") is True and
                          isinstance(row[field].get("path"), str) and bool(row[field]["path"])
                          and type(row[field].get("index")) is int and row[field]["index"] >= 0
                          and type(row[field].get("serial")) is int and row[field]["serial"] > 0
                          for row in calls for field in ("ability", "montage"))
    observed = bool(complete and calls and (start["schema"] >= 3 or live_references) and terminal.get("callsite_observed") is True)
    sources = {label: sum(row.get("source") == label for row in calls) for label in ("task_factory", "ready_for_activation")}
    if start["schema"] >= 4 and terminal_hooks is not None:
        by_label = {hook["label"]: hook for hook in terminal_hooks}
        if any(by_label.get(label, {}).get("matched") != count for label, count in sources.items()):
            raise ValueError("Per-hook match counts disagree with call rows")
    active_tags = sorted({row["current_event_tag"] for row in calls if start["schema"] >= 2 and
                          row["ability_active"] and row["current_event_tag"] not in ("", "None")})
    return {"session": key[0], "run": key[1], "capture_complete": complete,
            "player_outer_calls_observed": observed, "combat_verified": False,
            "retained_live_references": bool(calls and live_references),
            "calls": len(calls), "errors": errors, "terminal": terminal,
            "active_event_tags": active_tags,
            "sources": sources if start["schema"] >= 4 else None,
            "hooks": terminal_hooks, "host_stats": terminal.get("host_stats") if start["schema"] >= 4 else None,
            "control_seen": terminal.get("control_seen") if start["schema"] >= 4 else None,
            "func_swapped": {hook["label"]: hook["func_changed"] for hook in hooks} if hooks else None,
            "ability_paths": sorted({row["ability"]["path"] for row in calls if isinstance(row.get("ability"), dict) and row["ability"].get("path")}),
            "scope": "Read-only native task calls with player outer ancestry. Schema 3 can preserve transient name/value facts without a retained reference. Current event tags may be absent or historical, and do not establish a slot. Grant, avatar and swap compatibility are unverified."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.trace), indent=2))

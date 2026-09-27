#!/usr/bin/env python3
"""Summarize the last live capture without treating partial output as success."""
import argparse
import json
from pathlib import Path


def valid_registry_controls(controls: list) -> bool:
    if [r.get("control") for r in controls] != ["known_montage", "nonexistent_package", "known_folder"]:
        return False
    if any(r.get("passed") is not True or not isinstance(r.get("assets"), list) for r in controls):
        return False
    positive, negative, folder = controls
    return (positive.get("returned") is True and folder.get("returned") is True and
            bool(positive.get("expected_path")) and folder.get("expected_path") == positive["expected_path"] and
            any(asset.get("path") == positive["expected_path"] and asset.get("class") == "/Script/Engine.AnimMontage" for asset in positive["assets"]) and
            not negative["assets"] and any(asset.get("path") == folder["expected_path"] for asset in folder["assets"]))


def summarize(path: Path) -> dict:
    capture = None
    key = None
    with path.open(encoding="utf-8") as stream:
        for line_number, line in enumerate(iter(lambda: stream.readline(65537), ""), 1):
            if len(line) > 65536:
                raise ValueError(f"Oversized record at line {line_number}")
            if not line.strip():
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as error:
                raise ValueError(f"Invalid JSON at line {line_number}") from error
            if not isinstance(record, dict):
                raise ValueError(f"Invalid record at line {line_number}")
            if record.get("event") == "start":
                if record.get("schema_version") != 1 or record.get("probe") not in {"loaded_combat_01", "loaded_combat_02"}:
                    raise ValueError("Unsupported probe schema")
                capture = {"start": record, "player": None, "abilities": [], "interfaces": [], "errors": [], "end": None, "registry_controls": []}
                key = (record.get("session"), record.get("run"))
                continue
            if capture is None or (record.get("session"), record.get("run")) != key:
                continue
            if capture["end"] is not None:
                raise ValueError("Capture contains records after its end")
            event = record.get("event")
            if event == "player":
                capture["player"] = record
            elif event == "ability":
                if record.get("index") != len(capture["abilities"]):
                    raise ValueError("Ability records are missing or out of order")
                capture["abilities"].append(record)
            elif event == "interface":
                capture["interfaces"].append(record)
            elif event == "error":
                capture["errors"].append(record.get("message"))
            elif event == "registry_query":
                capture["registry_controls"].append(record)
            elif event == "end":
                capture["end"] = record
    if capture is None:
        raise ValueError("No capture found")
    end = capture["end"] or {}
    expected = (capture["player"] or {}).get("granted_abilities")
    count = len(capture["abilities"])
    complete = (end.get("state") == "complete" and not capture["errors"] and expected is not None and
                count == expected == end.get("abilities_read") == end.get("abilities_expected") and
                len(capture["interfaces"]) == 2 and end.get("maximum_step_us", 2001) <= 2000)
    if capture["start"]["probe"] == "loaded_combat_02":
        controls = capture["registry_controls"]
        complete = complete and valid_registry_controls(controls)
    montages = set()
    selectors = set()
    for row in capture["abilities"]:
        for ability in [row.get("ability"), *row.get("NonReplicatedInstances", []), *row.get("ReplicatedInstances", [])]:
            if not ability:
                continue
            montage = ability.get("montage")
            if montage:
                montages.add(montage["path"])
            for group in ability.get("selector", {}).values():
                for item in group.get("values", []):
                    obj = item.get("object")
                    if obj:
                        selectors.add(obj["path"])
                        if obj.get("montage"):
                            montages.add(obj["montage"]["path"])
    return {"session": key[0], "run": key[1], "state": end.get("state", "incomplete"),
            "verified_complete": complete, "grants_read": count, "grants_expected": expected,
            "selector_classes": sorted(selectors), "montages": sorted(montages),
            "maximum_step_us": end.get("maximum_step_us"), "errors": capture["errors"],
            "interfaces": capture["interfaces"], "registry_controls": capture["registry_controls"], "coverage": "loaded player grants only"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    args = parser.parse_args()
    report = summarize(args.input)
    print(json.dumps(report, indent=2))
    return 0 if report["verified_complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

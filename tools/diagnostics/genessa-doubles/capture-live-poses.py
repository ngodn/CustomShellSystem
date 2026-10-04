"""Capture doubles and menu state without changing gameplay. Python 3.14."""
import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from css import ROOT, atomic, processes
from css_live_snapshot import MOD, Probe


def context():
    value = json.loads((MOD / "runtime/status.json").read_text())
    inventory = value.get("inventory", {})
    return {
        "captured_ns": time.time_ns(),
        "status_mtime_ns": (MOD / "runtime/status.json").stat().st_mtime_ns,
        "applied": value.get("applied"),
        "shell": value.get("shell"),
        "mesh": value.get("mesh"),
        "menu_open": inventory.get("menu_open"),
        "astral": value.get("astral"),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=float, default=60)
    args = parser.parse_args()
    if not 5 <= args.seconds <= 180:
        parser.error("Use 5 to 180 seconds")
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "work"):
        parser.error("Evidence must stay inside the CSS work directory")
    pids = processes()
    if len(pids) != 1:
        parser.error("Expected exactly one running game")
    output.mkdir(parents=True, exist_ok=False)
    samples = active_samples = 0
    reason = "duration"
    try:
        with (output / "requests.jsonl").open("x") as log, (output / "samples.jsonl").open("x") as records:
            probe = Probe(log)
            until = time.monotonic() + args.seconds
            while time.monotonic() < until:
                if processes() != pids:
                    reason = "game_process_changed"
                    break
                before = context()
                observed = probe.send("astral.observe", poses=True)
                after = context()
                # Status publication is asynchronous. Bracketing reduces ambiguity
                # but cannot prove the world was unpaused at the exact pose sample.
                stable = all(before[key] == after[key] for key in ("applied", "shell", "mesh", "menu_open"))
                active = sum(bool(actor.get("active")) for actor in observed.get("actors", []))
                records.write(json.dumps({"before": before, "after": after,
                    "context_unchanged": stable, "observed": observed}) + "\n")
                records.flush()
                samples += 1
                active_samples += bool(active)
                time.sleep(0.35)
    except Exception:
        reason = "probe_failed"
        raise
    finally:
        atomic(output / "summary.json", {"pid": pids[0], "samples": samples,
            "active_samples": active_samples, "stop_reason": reason,
            "scope": "Read-only sampled poses with adjacent status snapshots. No input, camera, form or appearance changes."})
    print(f"Captured {samples} samples, {active_samples} with active doubles: {output}")


if __name__ == "__main__":
    main()

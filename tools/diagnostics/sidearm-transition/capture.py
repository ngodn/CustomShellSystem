"""Request one read-only snapshot through the installed MS2AttackProbe (Python 3.14).

Install this directory's main.lua as the probe's Scripts/RepairPrologue.lua first,
backing up the original. Restore it after diagnosis. This does not reload mods.
"""
import argparse
from pathlib import Path
import shutil
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("probe", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
request = f"repair_sidearm_{time.time_ns()}"
source = args.probe / f"repair-{request}.log"
args.output.parent.mkdir(parents=True, exist_ok=True)
(args.probe / "request.txt").write_text(request + "\n")
deadline = time.monotonic() + 8
while time.monotonic() < deadline:
    if source.exists():
        content = source.read_text()
        if "aimLayers=" in content or "ERROR " in content or "REFUSED " in content:
            shutil.copyfile(source, args.output)
            if "aimNode=" not in content or "ERROR " in content or "REFUSED " in content:
                raise SystemExit(f"Capture failed: {content.strip()}")
            print(args.output)
            break
    time.sleep(0.2)
else:
    raise SystemExit("No completed capture: game/probe may not be running or no sidearm is equipped")

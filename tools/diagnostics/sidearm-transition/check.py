"""Check the equipped machine gun's live aiming-layer assignment (Python 3.14)."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("capture", type=Path)
args = parser.parse_args()
rows = dict(line.split("=", 1) for line in args.capture.read_text().splitlines() if "=" in line)
if "class=BlueprintGeneratedClass /Game/Sparta/Core/Weapons/Player/MachineGun/WP_MachineGun.WP_MachineGun_C" not in rows.get("sidearm", ""):
    parser.error("Capture must show the equipped WP_MachineGun, with its class path")
node = rows.get("aimNode", "")
expected = "target=ABPL_Aim_MachineGun_C "
if expected not in node:
    raise SystemExit(f"FAIL: equipped machine gun lost its aiming layer: {node or 'missing aimNode capture'}")
print("PASS: equipped machine gun has ABPL_Aim_MachineGun")

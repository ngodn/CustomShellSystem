"""Install the passive doubles trial only while the game is closed. Python 3.14."""
import argparse
import json
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from css import GAME, ROOT, EXPECTED_UE4SS, atomic, copy_verified, processes, sha

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("pack", type=Path)
parser.add_argument("receipt", type=Path)
args = parser.parse_args()
pack = args.pack.resolve(strict=True)
receipt = args.receipt.resolve()
core = ROOT / "build/windows/css_core.dll"
runtime = GAME / "Binaries/Win64/ue4ss/Mods/CustomShellSystem"
mods = GAME / "Content/Paks/~mods"
name = "CSS_AstralSharedAssets_P"
target = mods / name
if processes():
    raise RuntimeError("Game is running; nothing installed")
if receipt.exists() or target.exists() or list(mods.rglob(name + ".*")):
    raise FileExistsError("Receipt or Astral package already exists; inspect before replacing")
if "CSS_INVENTORY_DEV:BOOL=ON" not in (ROOT / "build/windows/CMakeCache.txt").read_text():
    raise RuntimeError("Expected the developer build with passive trial arming")
for filename in ("material-check.json", "pose-check.json", "roundtrip.json"):
    if json.loads((pack / filename).read_text()).get("passed") is not True:
        raise RuntimeError("Shared package check failed: " + filename)
if json.loads((pack / "roundtrip.json").read_text()).get("container") != name:
    raise RuntimeError("Unexpected shared container identity")
selector = json.loads((runtime / "core.json").read_text())
if selector.get("abi") != 1 or Path(selector["file"]).name != selector["file"]:
    raise RuntimeError("Unexpected installed selector")
ue4ss = GAME / "Binaries/Win64/ue4ss/UE4SS.dll"
if sha(ue4ss) != EXPECTED_UE4SS:
    raise RuntimeError("Installed UE4SS does not match the pinned runtime")
core_hash = sha(core)
core_name = "css_core-astral-trial-" + core_hash[:16] + ".dll"
new_core = runtime / "cores" / core_name
if new_core.exists():
    raise FileExistsError(new_core)
protected = [ue4ss, runtime / "dlls/main.dll"]
protected += [p for p in (runtime / "state").rglob("*") if p.is_file()]
protected += list(mods.rglob("CSS_SharedAssets_P.*"))
protected_hashes = {str(p): sha(p) for p in protected}
receipt.mkdir(parents=True)
backup = receipt / "backup"
copy_verified(runtime / "core.json", backup / "core.json")
copy_verified(runtime / "cores" / selector["file"], backup / selector["file"])
assets = {name + suffix: sha(pack / (name + suffix)) for suffix in (".pak", ".utoc", ".ucas")}
try:
    if processes():
        raise RuntimeError("Game opened before installation")
    target.mkdir()
    for filename in assets:
        copy_verified(pack / filename, target / filename)
    copy_verified(core, new_core)
    if processes():
        raise RuntimeError("Game opened during staging; selector was not changed")
    if any(sha(Path(p)) != value for p, value in protected_hashes.items()):
        raise RuntimeError("A protected baseline file changed during staging")
    atomic(runtime / "core.json", {"abi": 1, "file": core_name})
    if sha(new_core) != core_hash or {p.name: sha(p) for p in target.iterdir()} != assets:
        raise RuntimeError("Installed hashes differ")
except Exception:
    copy_verified(backup / "core.json", runtime / "core.json")
    # Never delete files that a concurrently launched game might have mounted.
    if not processes():
        if target.exists():
            shutil.rmtree(target)
        new_core.unlink(missing_ok=True)
    raise
atomic(receipt / "deployment.json", {
    "installed": True, "live_verified": False, "trial_armed": False,
    "selector": {"abi": 1, "file": core_name}, "previous_selector": selector,
    "core_sha256": core_hash, "assets": assets, "asset_directory": str(target),
    "protected_hashes": protected_hashes, "source_pack": str(pack),
    "note": "Normal restart only. No game launch or live input was sent."
})
print("Passive doubles trial installed and hash-verified. Restart required.")

"""Install the audited private performance trial with the game closed. Python 3.14."""
import argparse
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from css import GAME, EXPECTED_UE4SS, atomic, copy_verified, processes, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    parser.add_argument("receipt", type=Path)
    args = parser.parse_args()
    pack = args.pack.resolve(strict=True)
    receipt = args.receipt.resolve()
    name = "CSS_LocomotionSharedAssets_P"
    mod = GAME / "Binaries/Win64/ue4ss/Mods/CustomShellSystem"
    cine = GAME / "Binaries/Win64/ue4ss/Mods/CSSX/extensions/eins0fx.cine"
    mods = GAME / "Content/Paks/~mods"
    target = mods / name
    stage = GAME / "Content" / (name + "-trial-stage")
    if processes():
        raise RuntimeError("Game is running; nothing installed")
    if receipt.exists() or stage.exists() or target.exists() or list(mods.rglob(name + ".*")):
        raise FileExistsError("Trial receipt, stage or installed pack exists; inspect before replacing")
    verified = json.loads((pack / "verification.json").read_text())
    if verified.get("passed") is not True or verified.get("exact_export_payloads") != 62:
        raise RuntimeError("Expected all 57 clips and five adapters to pass package readback")
    assets = verified["files"]
    if set(assets) != {name + suffix for suffix in (".pak", ".utoc", ".ucas")}:
        raise RuntimeError("Unexpected package files")
    for file, expected in assets.items():
        if sha(pack / name / file) != expected:
            raise RuntimeError("Package differs from readback: " + file)
    for file, expected in verified["dependencies"].items():
        if sha(Path(file)) != expected:
            raise RuntimeError("An installed outfit changed after adapter verification: " + file)
    selector = json.loads((mod / "core.json").read_text())
    extension = json.loads((cine / "extension.json").read_text())
    if selector.get("abi") != 1 or Path(selector["file"]).name != selector["file"]:
        raise RuntimeError("Unexpected CSS loader selector")
    if extension.get("id") != "eins0fx.cine" or extension.get("api") != 3 or Path(extension["entry"]).name != extension["entry"]:
        raise RuntimeError("Unexpected installed CINE manifest")
    original_menu = ROOT / "work/cine-animation-port/cine-before-performance/extensions/cine/menu.json"
    if sha(cine / "menu.json") != sha(original_menu):
        raise RuntimeError("Installed CINE menu differs from the preserved integration baseline")
    ue4ss = GAME / "Binaries/Win64/ue4ss/UE4SS.dll"
    if sha(ue4ss) != EXPECTED_UE4SS:
        raise RuntimeError("Unexpected UE4SS runtime")
    core = ROOT / "build/windows/css_core.dll"
    cine_dll = ROOT / "build/cine-cssx-win/cine.dll"
    core_hash, cine_hash = sha(core), sha(cine_dll)
    core_file = "css_core-performance-" + core_hash[:16] + ".dll"
    cine_file = "cine-performance-" + cine_hash[:16] + ".dll"
    if (mod / "cores" / core_file).exists() or (cine / cine_file).exists():
        raise FileExistsError("Candidate DLL already staged")
    protected = [ue4ss, mod / "dlls/main.dll"]
    protected += [p for p in (mod / "state").rglob("*") if p.is_file()]
    protected += [p for p in (cine.parents[1] / "state").rglob("*") if p.is_file()]
    before = {str(p): sha(p) for p in protected}
    receipt.mkdir(parents=True)
    backup = receipt / "backup"
    for source, relative in (
        (mod / "core.json", "core.json"), (mod / "cores" / selector["file"], selector["file"]),
        (cine / "extension.json", "cine-extension.json"), (cine / "menu.json", "cine-menu.json"),
        (cine / extension["entry"], extension["entry"]),
    ):
        copy_verified(source, backup / relative)
    copy_verified(core, receipt / core_file)
    copy_verified(cine_dll, receipt / cine_file)
    copy_verified(ROOT / "extensions/cine/menu.json", receipt / "cine-menu.json")
    switched = False
    try:
        stage.mkdir()
        for file in assets:
            copy_verified(pack / name / file, stage / file)
        copy_verified(receipt / core_file, mod / "cores" / core_file)
        copy_verified(receipt / cine_file, cine / cine_file)
        if processes():
            raise RuntimeError("Game opened during staging; selectors were not changed")
        if any(sha(Path(p)) != digest for p, digest in before.items()):
            raise RuntimeError("Protected files changed during staging")
        stage.rename(target)
        switched = True
        copy_verified(receipt / "cine-menu.json", cine / "menu.json")
        atomic(cine / "extension.json", {**extension, "entry": cine_file})
        atomic(mod / "core.json", {**selector, "file": core_file})
        if sha(mod / "cores" / core_file) != core_hash or sha(cine / cine_file) != cine_hash:
            raise RuntimeError("Installed DLL hash mismatch")
        if {p.name: sha(p) for p in target.iterdir()} != assets:
            raise RuntimeError("Installed container hash mismatch")
        if any(sha(Path(p)) != digest for p, digest in before.items()):
            raise RuntimeError("Protected files changed during installation")
    except Exception as error:
        if processes():
            atomic(receipt / "install-error.json", {"error": str(error), "selectors_changed": switched,
                "note": "Game opened concurrently. Files retained to avoid hot reload; close the game before rollback."})
            raise
        if switched:
            copy_verified(backup / "core.json", mod / "core.json")
            copy_verified(backup / "cine-extension.json", cine / "extension.json")
            copy_verified(backup / "cine-menu.json", cine / "menu.json")
        for directory in ((target, stage) if switched else (stage,)):
            if directory.exists():
                shutil.rmtree(directory)
        (mod / "cores" / core_file).unlink(missing_ok=True)
        (cine / cine_file).unlink(missing_ok=True)
        raise
    atomic(receipt / "deployment.json", {
        "installed": True, "live_verified": False, "game_was_closed": True,
        "core": core_file, "core_sha256": core_hash, "previous_core": selector,
        "cine": cine_file, "cine_sha256": cine_hash, "previous_cine": extension,
        "assets": assets, "asset_directory": str(target), "protected_hashes": before,
        "note": "Private test only. No launch, reload, input or recording was requested."
    })
    print("Private performance trial installed and hash-verified. Game remains closed.")


if __name__ == "__main__":
    main()

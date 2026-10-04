"""Check eye-section visibility, controls, animation and native ghost tint."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import OpenEXR


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    root = args.directory.resolve()
    manifest = json.loads((root / "captures.json").read_text())
    if not manifest["source_files_unchanged"] or not manifest["compilation"]["passed"]:
        raise ValueError("Unverified capture sources or shaders")
    if not manifest["mesh"].startswith("/Game/CSS/UnholyGenessa/SK_EveW3."):
        raise ValueError("Expected authored Genessa eye geometry")
    groups = {(kind, form): {} for kind in ("eye", "smoke") for form in ("faithful", "stray")}
    for row in manifest["images"]:
        key, state = (row["kind"], row["form"]), row["state"]
        path = (root / row["image"]).resolve()
        if key not in groups or state in groups[key] or path.parent != root or path.suffix != ".exr":
            raise ValueError("Unexpected capture identity or path")
        if hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
            raise ValueError("Capture hash mismatch")
        with OpenEXR.File(str(path)) as image:
            rgb = image.channels()["RGBA"].pixels[:, :, :3].astype(np.float64)
        if rgb.shape != (manifest["resolution"], manifest["resolution"], 3) or not np.isfinite(rgb).all():
            raise ValueError("Invalid capture dimensions or pixels")
        groups[key][state] = rgb
    results = []
    for (kind, form), images in groups.items():
        expected = {"source", "full", "half", "zero", "removed", "ghost_time",
                    "intensity_zero", "color_red", "color_blue"}
        if kind == "smoke":
            expected |= {"smoke_time", "smoke_opacity_zero"}
        if set(images) != expected:
            raise ValueError("Incomplete capture states")
        full, half = images["full"], images["half"]
        tint = {}
        for state in ("full", "color_red", "color_blue", "intensity_zero"):
            rgb = images[state].sum(axis=(0, 1))
            tint[state] = bool(rgb[2] > rgb[1] > rgb[0] if form == "faithful"
                               else rgb[0] > 3 * max(rgb[1:]))
        checks = {
            "source_visible": bool(images["source"].max() > 1e-5),
            "ghost_visible": bool(full.max() > 1e-5),
            "partial_fade": bool(0 < half.sum() < full.sum()),
            "full_fade_empty": bool(not images["zero"].any() and not images["removed"].any()),
            "native_time_changes": bool(np.max(np.abs(images["ghost_time"] - full)) > 1e-5),
            "intensity_changes": bool(full.sum() > images["intensity_zero"].sum()),
            "color_changes": bool(np.max(np.abs(images["color_red"] - images["color_blue"])) > 1e-5),
            "native_tint": all(tint.values()),
        }
        if kind == "smoke":
            checks["smoke_time_changes"] = bool(np.max(np.abs(images["smoke_time"] - full)) > 1e-5)
            checks["smoke_opacity_zero_empty"] = bool(not images["smoke_opacity_zero"].any())
        results.append({"kind": kind, "form": form, "checks": checks,
                        "tint_by_state": tint, "passed": all(checks.values())})
    result = {"passed": all(row["passed"] for row in results), "groups": results,
              "scope": manifest["scope"]}
    with (root / "hdr-check.json").open("x") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

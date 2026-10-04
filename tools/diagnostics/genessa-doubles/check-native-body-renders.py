"""Check cutout boundaries against known texture alpha, then check ghost fades."""
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
    root = args.directory.resolve(strict=True)
    report = json.loads((root / "captures.json").read_text())
    if not report["source_files_unchanged"] or not report["compilation"]["passed"]:
        raise ValueError("Compilation or source check failed")
    states = {"opaque", "full", "half", "zero", "removed", "below_clip", "at_clip",
              "above_clip", "red_channel", "zero_channel", "alpha_zero", "opaque_alpha_zero", "time"}
    groups = {form: {} for form in ("faithful", "stray")}
    for row in report["images"]:
        if row["form"] not in groups or row["state"] not in states or row["state"] in groups[row["form"]]:
            raise ValueError("Unexpected or duplicate capture")
        path = (root / row["image"]).resolve(strict=True)
        if path.parent != root or path.suffix != ".exr":
            raise ValueError("Expected an EXR in the capture directory")
        if hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
            raise ValueError(f"Capture changed: {path}")
        with OpenEXR.File(str(path)) as image:
            rgb = image.channels()["RGBA"].pixels[:, :, :3].astype(np.float64)
        if rgb.shape != (report["resolution"], report["resolution"], 3) or not np.isfinite(rgb).all():
            raise ValueError("Invalid image dimensions or pixels")
        groups[row["form"]][row["state"]] = rgb
    results = []
    for form, images in groups.items():
        if set(images) != states:
            raise ValueError(f"Incomplete captures for {form}")
        background = images["removed"]
        if np.any(background):
            raise ValueError("Expected a black backdrop")
        full = images["full"]
        color = full.mean(axis=(0, 1))
        checks = {"visible": bool(full.max() > 1e-5),
            "partial_fade": bool(images["half"].max() > 1e-5 and np.max(np.abs(images["half"] - full)) > 1e-5),
            "animated": bool(np.max(np.abs(images["time"] - full)) > 1e-5),
            "native_tint": bool(color[2] > color[1] > color[0] if form == "faithful" else color[0] > 3 * max(color[1:]))}
        for state in ("zero", "below_clip", "zero_channel", "alpha_zero"):
            checks[state + "_empty"] = bool(np.array_equal(images[state], background))
        # At the native clip boundary equality survives; only negative values discard.
        for state in ("opaque", "at_clip", "above_clip", "red_channel", "opaque_alpha_zero"):
            checks[state + "_matches_full"] = bool(np.array_equal(images[state], full))
        results.append({"form": form, "passed": all(checks.values()), "checks": checks,
                        "mean_foreground_rgb": color.tolist()})
    result = {"passed": all(row["passed"] for row in results), "forms": results,
              "scope": report["scope"]}
    with (root / "hdr-check.json").open("x") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

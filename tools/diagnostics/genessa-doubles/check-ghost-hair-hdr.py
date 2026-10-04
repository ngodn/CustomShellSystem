"""Verify linear ghost coverage composition using UE's half-float EXR captures."""
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
    if not (manifest["float_capture"] and manifest["source_files_unchanged"]
            and manifest["compilation"]["passed"]):
        raise ValueError("Expected successful HDR captures with unchanged sources")
    sources = [f"/Game/CSS/SeduXtress/{part}/MI_ShellKeeper_Hair_01" for part in ("Hair", "Tail")]
    sources += [f"/Game/CSS/CommanderWhite/MaterialY/MI_Layer_{slot:02}" for slot in (29, 30, 31, 32, 35, 36)]
    controls = {"coverage": (1., 0.), "uncut": (1., 0.), "full_t0": (1., 0.),
                "full_t1": (1., 1.), "half": (.5, 0.), "zero": (0., 0.), "removed": (0., 0.)}
    groups = {(source, form): {} for source in sources for form in ("faithful", "stray")}
    for row in manifest["images"]:
        key = row["source"], row["form"]
        state = row["state"]
        if key not in groups or state not in controls or state in groups[key]:
            raise ValueError("Unknown or duplicate capture")
        if (row["opacity"], row["time"]) != controls[state]:
            raise ValueError("Mismatched capture controls")
        path = (root / row["image"]).resolve()
        if path.parent != root or path.suffix != ".exr":
            raise ValueError("Expected EXR inside the capture directory")
        if hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
            raise ValueError(f"Changed capture: {path}")
        with OpenEXR.File(str(path)) as image:
            rgb = image.channels()["RGBA"].pixels[:, :, :3].astype(np.float64)
        if rgb.shape != (manifest["resolution"], manifest["resolution"], 3) or not np.isfinite(rgb).all():
            raise ValueError("Invalid dimensions or nonfinite pixels")
        groups[key][state] = rgb
    results = []
    for (source, form), images in groups.items():
        if set(images) != set(controls):
            raise ValueError(f"Incomplete captures: {source} {form}")
        background = images["removed"]
        if np.any(background):
            raise ValueError("Fixture requires a black backdrop")
        # The reference graph emits 100 white through the untouched source alpha.
        coverage = images["coverage"] / 100.
        expected = images["uncut"] * coverage
        difference = np.abs(images["full_t0"] - expected)
        # Allow half-float render/blend rounding across the three independent captures.
        product_matches = bool(np.all(difference <= 2e-6 + .003 * np.abs(expected)))
        lit = images["uncut"].max(axis=2) > 1e-5
        holes = (coverage[:, :, 0] == 0) & lit
        attenuated = (coverage[:, :, 0] < .99) & lit
        leaks = {state: int(np.count_nonzero(images[state].max(axis=2)[holes] > 1e-7))
                 for state in ("full_t0", "full_t1", "half")}
        color = images["full_t0"].mean(axis=(0, 1))
        tint = bool(color[2] > color[1] > color[0] if form == "faithful"
                    else color[0] > 3 * max(color[1:]))
        checks = {
            "linear_coverage_product": product_matches,
            "coverage_not_clamped_to_one": bool(images["coverage"].max() > 1.),
            "visible": bool(images["full_t0"].max() > 1e-5),
            "half_visible": bool(images["half"].max() > 1e-5),
            "partial_fade": bool(np.max(np.abs(images["half"] - images["full_t0"])) > 1e-5),
            "animated": bool(np.max(np.abs(images["full_t1"] - images["full_t0"])) > 1e-5),
            "zero_equals_removed": bool(np.array_equal(images["zero"], background)),
            "coverage_reduces_ghost": bool(np.count_nonzero(attenuated) > 100),
            "holes_remain_empty": not any(leaks.values()), "native_tint": tint,
        }
        results.append({"source": source, "form": form, "passed": all(checks.values()),
            "checks": checks, "maximum_product_error": float(difference.max()),
            "hole_pixels": int(np.count_nonzero(holes)), "leaked_pixels": leaks,
            "attenuated_pixels": int(np.count_nonzero(attenuated)), "mean_foreground_rgb": color.tolist()})
    passed = all(row["passed"] for row in results)
    result = {"passed": passed, "materials": results, "scope": manifest["scope"],
              "versions": {"OpenEXR": OpenEXR.__version__, "numpy": np.__version__}}
    with (root / "hdr-check.json").open("x") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": passed, "groups": len(results)}))
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

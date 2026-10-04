"""Check lit source plus ghost against separately rendered terms and edit controls."""
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
    prefix = "/Game/CSS/UnholyGenessa/"
    sources = [prefix + p for p in ("Mat1/M_Trim", "Mat10/M_Trim", "Mat12/M_Metal",
                                    "Fabric02/M_Fabric", "Fabric02/M_Silk")]
    sources += ["/Game/CSS/SeduXtress/Hair/MI_ShellKeeper_Hair_01",
                "/Game/CSS/CommanderWhite/MaterialY/MI_Layer_32"]
    controls = {state: (1., 0.) for state in ("coverage", "ghost_only", "surface_only", "combined",
                                           "palette_red", "palette_blue", "glow")}
    controls.update({"time": (1., 1.), "half": (.5, 0.), "zero": (0., 0.), "removed": (0., 0.)})
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
        expected_states = set(controls)
        if source not in sources[:5]:
            expected_states -= {"palette_red", "palette_blue", "glow"}
        elif source == sources[4]:
            expected_states.remove("glow")
        if set(images) != expected_states:
            raise ValueError(f"Incomplete captures: {source} {form}")
        background = images["removed"]
        if np.any(background):
            raise ValueError("Fixture requires a black backdrop")
        expected = images["surface_only"] + images["ghost_only"]
        difference = np.abs(images["combined"] - expected)
        # Three half-float render targets incur independent rounding.
        sum_matches = bool(np.all(difference <= 2e-6 + .003 * np.abs(expected)))
        holes = images["coverage"].max(axis=2) == 0
        compared = {state for state in ("combined", "surface_only", "ghost_only", "palette_red", "palette_blue", "glow") if state in images}
        leaks = {state: int(np.count_nonzero(images[state].max(axis=2)[holes] > 1e-7))
                 for state in sorted(compared)}
        color = images["ghost_only"].mean(axis=(0, 1))
        tint = bool(color[2] > color[1] > color[0] if form == "faithful"
                    else color[0] > 3 * max(color[1:]))
        checks = {
            "source_plus_ghost": sum_matches,
            "coverage_not_clamped_to_one": bool(images["coverage"].max() > 1.),
            "source_visible": bool(images["surface_only"].max() > 1e-5),
            "combined_visible": bool(images["combined"].max() > 1e-5),
            "half_visible": bool(images["half"].max() > 1e-5),
            "partial_fade": bool(np.max(np.abs(images["half"] - images["combined"])) > 1e-5),
            "animated": bool(np.max(np.abs(images["time"] - images["combined"])) > 1e-5),
            "zero_equals_removed": bool(np.array_equal(images["zero"], background)),
            "holes_remain_empty": not any(leaks.values()), "native_term_tint": tint,
        }
        if "palette_red" in images:
            change = images["palette_red"] - images["palette_blue"]
            checks["palette_changes"] = bool(change[:, :, 0].max() > 1e-5 and change[:, :, 2].min() < -1e-5)
        if "glow" in images:
            checks["glow_changes"] = bool(np.max(images["glow"] - images["combined"]) > 1e-5)
        results.append({"source": source, "form": form, "passed": all(checks.values()),
            "checks": checks, "maximum_sum_error": float(difference.max()),
            "hole_pixels": int(np.count_nonzero(holes)), "leaked_pixels": leaks,
            "mean_ghost_rgb": color.tolist()})
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

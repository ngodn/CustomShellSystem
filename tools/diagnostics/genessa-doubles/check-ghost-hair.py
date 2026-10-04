"""Compare ghost hair against an independent source-alpha coverage render."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageStat


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    root = args.directory.resolve()
    report = json.loads((root / "captures.json").read_text())
    if not report["source_files_unchanged"] or not report["compilation"]["passed"]:
        raise ValueError("Compilation or source integrity failed")
    controls = {"coverage": (1., 0.), "uncut": (1., 0.), "full_t0": (1., 0.),
                "full_t1": (1., 1.), "half": (.5, 0.), "zero": (0., 0.), "removed": (0., 0.)}
    expected = [f"/Game/CSS/SeduXtress/{part}/MI_ShellKeeper_Hair_01" for part in ("Hair", "Tail")]
    expected += [f"/Game/CSS/CommanderWhite/MaterialY/MI_Layer_{slot:02}" for slot in (29, 30, 31, 32, 35, 36)]
    groups = {(source, form): {} for source in expected for form in ("faithful", "stray")}
    for row in report["images"]:
        key = row["source"], row["form"]
        state = row["state"]
        if key not in groups or state not in controls or state in groups[key]:
            raise ValueError("Unknown or duplicate capture")
        if (row["opacity"], row["time"]) != controls[state]:
            raise ValueError("Mismatched capture controls")
        path = (root / row["image"]).resolve()
        if path.parent != root or hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
            raise ValueError("Capture path or hash mismatch")
        with Image.open(path) as image:
            groups[key][state] = image.convert("RGB")
    results = []
    for (source, form), images in groups.items():
        if set(images) != set(controls) or len({image.size for image in images.values()}) != 1:
            raise ValueError(f"Incomplete or inconsistent captures: {source} {form}")
        differences = {state: ImageChops.difference(image, images["removed"])
                       for state, image in images.items()}
        pixels = {state: list(image.get_flattened_data()) for state, image in differences.items()}
        holes = [i for i, (mask, raw) in enumerate(zip(pixels["coverage"], pixels["uncut"]))
                 if max(mask) == 0 and max(raw) > 3]
        leaks = {state: sum(max(pixels[state][i]) > 0 for i in holes)
                 for state in ("full_t0", "full_t1", "half")}
        comparisons = {state: differences[state].getbbox() is not None
                       for state in ("coverage", "uncut", "full_t0", "half")}
        comparisons["animated"] = ImageChops.difference(images["full_t0"], images["full_t1"]).getbbox() is not None
        comparisons["partial_fade"] = ImageChops.difference(images["full_t0"], images["half"]).getbbox() is not None
        comparisons["zero_equals_removed"] = differences["zero"].getbbox() is None
        color = ImageStat.Stat(differences["full_t0"]).mean
        tint = color[2] > color[1] > color[0] if form == "faithful" else color[0] > 3 * max(color[1:])
        passed = all(comparisons.values()) and tint and len(holes) > 100 and not any(leaks.values())
        results.append({"source": source, "form": form, "passed": passed,
            "tested_hole_pixels": len(holes), "leaked_pixels": leaks,
            "comparisons": comparisons, "mean_foreground_rgb": color, "expected_tint": tint})
    passed = all(row["passed"] for row in results)
    result = {"passed": passed, "materials": results, "scope": report["scope"]}
    with (root / "pixel-check.json").open("x") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": passed, "groups": len(results)}))
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

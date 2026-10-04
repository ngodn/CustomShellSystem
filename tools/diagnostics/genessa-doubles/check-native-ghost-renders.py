"""Check the isolated native-ghost sphere captures, without claiming equivalence."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageStat


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    report = json.loads((args.directory / "captures.json").read_text())
    if not report["source_files_unchanged"] or not report["compilation"]["passed"]:
        raise ValueError("Source or compilation check failed")
    states = {"full_t0": (1., 0.), "full_t1": (1., 1.), "half": (.5, 0.),
              "zero": (0., 0.), "removed": (0., 0.)}
    forms = {"faithful": {}, "stray": {}}
    for row in report["images"]:
        if row["form"] not in forms or row["state"] not in states:
            raise ValueError("Unexpected form or capture state")
        images = forms[row["form"]]
        if row["state"] in images or (row["opacity"], row["time"]) != states[row["state"]]:
            raise ValueError("Duplicate or mismatched capture controls")
        path = args.directory / row["image"]
        if path.resolve().parent != args.directory.resolve():
            raise ValueError("Image outside capture directory")
        if hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
            raise ValueError(f"Capture changed: {path}")
        with Image.open(path) as im:
            images[row["state"]] = im.convert("RGB")
    results = []
    for form, images in forms.items():
        if set(images) != set(states) or len({im.size for im in images.values()}) != 1:
            raise ValueError(f"Incomplete or inconsistent images for {form}")
        comparisons = {}
        for a, b in (("zero", "removed"), ("full_t0", "removed"),
                     ("half", "full_t0"), ("half", "zero"), ("full_t0", "full_t1")):
            difference = ImageChops.difference(images[a], images[b])
            comparisons[a + "_" + b] = {"equal": difference.getbbox() is None,
                                        "mean_rgb_error": ImageStat.Stat(difference).mean}
        color = ImageStat.Stat(ImageChops.difference(images["full_t0"], images["removed"])).mean
        tint = color[2] > color[1] > color[0] if form == "faithful" else color[0] > 3 * max(color[1:])
        passed = comparisons["zero_removed"]["equal"] and tint and all(
            not row["equal"] for key, row in comparisons.items() if key != "zero_removed")
        results.append({"form": form, "passed": passed, "comparisons": comparisons,
                        "mean_foreground_rgb": color, "expected_tint": tint})
    passed = all(row["passed"] for row in results)
    result = {"passed": passed, "forms": results,
              "scope": "Reconstructed graph on a Vulkan sphere. No native-render comparison, outfit support or DX12 proof."}
    with (args.directory / "pixel-check.json").open("x") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": passed, "forms": len(results)}))
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

"""Check actual render pixels for disappearance and nontrivial intermediate fade."""
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
    groups = {}
    for row in report["images"]:
        key = row.get("source", row.get("view"))
        group = groups.setdefault(key, {})
        if row["state"] in group:
            raise ValueError(f"Duplicate state for {key}: {row['state']}")
        image = args.directory / row["image"]
        if image.resolve().parent != args.directory.resolve():
            raise ValueError("Image outside render directory")
        if hashlib.sha256(image.read_bytes()).hexdigest() != row["sha256"]:
            raise ValueError(f"Capture changed: {image}")
        with Image.open(image) as loaded:
            group[row["state"]] = loaded.convert("RGB")
    if not groups:
        raise ValueError("No captures")
    rows = []
    for key, images in groups.items():
        if set(images) != {"original", "full", "half", "zero", "removed"}:
            raise ValueError(f"Incomplete capture set: {key}")
        if len({im.size for im in images.values()}) != 1:
            raise ValueError(f"Different image sizes: {key}")
        comparisons = {}
        for a, b in (("zero", "removed"), ("full", "removed"),
                     ("half", "full"), ("half", "zero"), ("full", "original")):
            difference = ImageChops.difference(images[a], images[b])
            comparisons[f"{a}_{b}"] = {
                "equal": difference.getbbox() is None,
                "mean_rgb_error": ImageStat.Stat(difference).mean}
        passed = comparisons["zero_removed"]["equal"] and all(
            not comparisons[key]["equal"] for key in ("full_removed", "half_full", "half_zero"))
        rows.append({"group": key, "passed": passed, "comparisons": comparisons})
    passed = all(row["passed"] for row in rows)
    result = {"passed": passed, "groups": rows,
              "scope": "Coverage/fade pixels only; no claim of native ghost effect or in-game compatibility."}
    with (args.directory / "pixel-check.json").open("x") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": passed, "groups": len(rows)}))
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

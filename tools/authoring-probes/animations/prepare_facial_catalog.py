"""Describe verified shared Eve faces without replacing the installed catalog."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LABELS = {
    "Angry": "Angry", "Contempt": "Contempt", "Daydream": "Daydream", "Gloomy": "Gloomy",
    "Happy": "Happy", "Kiss": "Kiss", "Laugh": "Laugh", "LookDown": "Look Down",
    "Netural": "Neutral", "Oops": "Oops", "Puffed": "Puffed", "Sad": "Sad", "Serious": "Serious",
    "Sigh": "Sigh", "Smile": "Smile", "Sob": "Sob", "Sulk": "Sulk", "Surprised": "Surprised",
    "Tease-ya": "Tease-ya", "Wink": "Wink", "Zzz": "Zzz",
}


def digest(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("work", type=Path)
    parser.add_argument("--readback", type=Path, required=True)
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    if not work.is_relative_to(ROOT / "work/cine-animation-port"):
        raise ValueError("Use the private animation workspace")
    paths = [work / "probe/receipt.json", work / "facial-stage-verify.json", args.readback.resolve(strict=True),
             work.parent / "facial-export-v1/facial-shapes.json"]
    receipt, staging, readback, shapes = [json.loads(path.read_text()) for path in paths]
    if not staging.get("complete") or not readback.get("passed") or len(readback["results"]) != 6:
        raise ValueError("Fresh editor and cooked readback must pass")
    protected = {str(path): digest(path) for path in paths}
    for group in ("protected", "cooked", "containers"):
        protected.update(receipt[group])
    if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
        raise ValueError("Facial evidence or protected source changed")
    sources = [(Path(path), checksum) for path, checksum in receipt["protected"].items()
               if Path(path).name == "CSS_EveStellarBlade_eins0fx_P.utoc"]
    if len(sources) != 1:
        raise ValueError("Expected exactly one verified Eve source revision")
    source, checksum = sources[0]
    pairs = {(row["source"], row["target"]) for row in staging["results"]}
    if pairs != {(row["source"], row["target"]) for row in readback["results"]}:
        raise ValueError("Editor and cooked mesh sets differ")
    morphs = sorted(item["name"] for item in shapes["morph_targets"])
    if len(morphs) != 99 or len(set(morphs)) != 99:
        raise ValueError("Unexpected facial morph names")
    expressions = []
    found = set()
    for entry in shapes["expressions"]:
        name = entry["name"].removeprefix("PT_").removesuffix("_EVE")
        if name not in LABELS or name in found:
            raise ValueError("Unknown or duplicate facial source")
        found.add(name)
        label = LABELS[name]
        identity = label.lower().replace(" ", "-")
        expressions.append({"id": "css.eve.face." + identity, "name": label, "weights": entry["weights"]})
    if found != set(LABELS):
        raise ValueError("Missing source expression")
    adapters = []
    for row in readback["results"]:
        if not row["passed"] or {m["name"] for m in row["morphs"]} != set(morphs):
            raise ValueError("Incomplete cooked morph readback")
        skeleton = row["Skeleton"]
        if not skeleton.startswith("Skeleton'/Game/") or not skeleton.endswith("'"):
            raise ValueError("Unexpected cooked skeleton binding")
        reference = lambda path: path + "." + path.rsplit("/", 1)[-1]
        adapters.append({"source_outfit": "eins0fx.seduxtress", "source_mesh": reference(row["source"]),
            "target_mesh": reference(row["target"]), "skeleton": skeleton[len("Skeleton'"):-1],
            "source_toc_sha256": checksum, "source_bulk_bytes": source.with_suffix(".ucas").stat().st_size})
    rig = {"id": "css.eve.face", "morphs": morphs, "expressions": expressions, "adapters": adapters}
    with (work / "facial-rig.json").open("x") as file:
        json.dump(rig, file, indent=2, allow_nan=False)
        file.write("\n")
    with (work / "facial-catalog-evidence.json").open("x") as file:
        json.dump({"passed": True, "protected": protected, "rig_sha256": digest(work / "facial-rig.json"),
            "installed": False, "scope": "Cooked, revision-bound catalog. Runtime activation remains pending."}, file, indent=2)
        file.write("\n")
    print(work / "facial-rig.json")


if __name__ == "__main__":
    main()

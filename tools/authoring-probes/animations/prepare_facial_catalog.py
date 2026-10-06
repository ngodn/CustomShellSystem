"""Describe verified shared faces without replacing the installed catalog."""
import argparse
import hashlib
import json
from pathlib import Path
from prepare_performance_curves import native_performance
from prepare_blender_face import verified_performance

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
    parser.add_argument("--outfit", choices=("eve", "commander-white"), default="eve")
    parser.add_argument("--readback", type=Path, required=True)
    parser.add_argument("--shapes", type=Path, help="Verified facial interchange for this cook")
    parser.add_argument("--performance", action="append", default=[], type=Path,
                        help="Prepared source curves with a canonical native performance entry")
    parser.add_argument("--blender-performance", action="append", default=[], type=Path,
                        help="Prepared evaluated Blender facial source")
    parser.add_argument("--output", type=Path, help="New metadata directory; reuse the verified cook")
    args = parser.parse_args()
    is_eve = args.outfit == "eve"
    mesh_count = 6 if is_eve else 1
    rig_id = "css.eve.face" if is_eve else "css.commanderwhite.face"
    outfit_id = "eins0fx.seduxtress" if is_eve else "eins0fx.commanderwhite"
    container_name = "CSS_EveStellarBlade_eins0fx_P.utoc" if is_eve else "CSS_CommanderWhite_eins0fx_P.utoc"
    labels = LABELS if is_eve else {name: name for name in
        ("Neutral", "Afraid", "Angry", "Flirting", "Frown", "Shock", "Smile", "Open Smile", "Surprised")}
    if not is_eve and (args.performance or args.blender_performance):
        raise ValueError("Commander White performance mappings require separate source verification")
    work = args.work.resolve(strict=True)
    if not work.is_relative_to(ROOT / "work/cine-animation-port"):
        raise ValueError("Use the private animation workspace")
    output = args.output.resolve() if args.output else work
    if not output.is_relative_to(ROOT / "work/cine-animation-port"):
        raise ValueError("Keep catalog metadata in the private animation workspace")
    paths = [work / "probe/receipt.json", work / "facial-stage-verify.json", args.readback.resolve(strict=True),
             (args.shapes or work.parent / "facial-export-v1/facial-shapes.json").resolve(strict=True)]
    receipt, staging, readback, shapes = [json.loads(path.read_text()) for path in paths]
    if not staging.get("complete") or not readback.get("passed") or len(readback["results"]) != mesh_count or not readback.get("source_deltas_checked"):
        raise ValueError("Fresh editor and cooked readback must pass")
    protected = {str(path): digest(path) for path in paths}
    for group in ("protected", "cooked", "containers"):
        protected.update(receipt[group])
    if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
        raise ValueError("Facial evidence or protected source changed")
    sources = [(Path(path), checksum) for path, checksum in receipt["protected"].items()
               if Path(path).name == container_name]
    if len(sources) != 1:
        raise ValueError("Expected exactly one verified outfit source revision")
    source, checksum = sources[0]
    pairs = {(row["source"], row["target"]) for row in staging["results"]}
    if pairs != {(row["source"], row["target"]) for row in readback["results"]}:
        raise ValueError("Editor and cooked mesh sets differ")
    morphs = sorted(item["name"] for item in shapes["morph_targets"])
    if not 1 <= len(morphs) <= 128 or len({name.casefold() for name in morphs}) != len(morphs):
        raise ValueError("Unexpected facial morph names")
    if readback.get("shapes_sha256") != digest(paths[3]):
        raise ValueError("Cooked readback used a different facial interchange")
    expressions = []
    found = set()
    for entry in shapes["expressions"]:
        name = entry["name"].removeprefix("PT_").removesuffix("_EVE")
        if name not in labels or name in found:
            raise ValueError("Unknown or duplicate facial source")
        found.add(name)
        label = labels[name]
        identity = label.lower().replace(" ", "-")
        expressions.append({"id": rig_id + "." + identity, "name": label, "weights": entry["weights"]})
    if found != set(labels):
        raise ValueError("Missing source expression")
    adapters = []
    for row in readback["results"]:
        if not row["passed"] or {m["name"] for m in row["morphs"]} != set(morphs):
            raise ValueError("Incomplete cooked morph readback")
        skeleton = row["Skeleton"]
        if not skeleton.startswith("Skeleton'/Game/") or not skeleton.endswith("'"):
            raise ValueError("Unexpected cooked skeleton binding")
        reference = lambda path: path + "." + path.rsplit("/", 1)[-1]
        adapters.append({"source_outfit": outfit_id, "source_mesh": reference(row["source"]),
            "target_mesh": reference(row["target"]), "skeleton": skeleton[len("Skeleton'"):-1],
            "source_toc_sha256": checksum, "source_bulk_bytes": source.with_suffix(".ucas").stat().st_size})
    rig = {"id": rig_id, "morphs": morphs, "expressions": expressions, "adapters": adapters}
    performances = []
    identities = set()
    for curve_path in args.performance:
        curve_path = curve_path.resolve(strict=True)
        prepared = json.loads(curve_path.read_text())
        entry = prepared["performance"]
        inputs = prepared["inputs"]
        if inputs.get(str(paths[3])) != digest(paths[3]):
            raise ValueError("Performance uses a different facial shape set")
        sources = [Path(path) for path in inputs if Path(path) != paths[3]]
        if len(sources) != 1 or any(digest(Path(path)) != sha for path, sha in inputs.items()):
            raise ValueError("Performance source evidence changed")
        if (native_performance(json.loads(sources[0].read_text()), morphs, entry["clip"]) != entry or
                entry["clip"] in identities):
            raise ValueError("Changed or duplicate facial performance")
        identities.add(entry["clip"])
        performances.append(entry)
        protected.update(inputs)
        protected[str(curve_path)] = digest(curve_path)
    for curve_path in args.blender_performance:
        curve_path = curve_path.resolve(strict=True)
        prepared = json.loads(curve_path.read_text())
        entry = verified_performance(prepared,paths[3],morphs)
        if entry['clip'] in identities:
            raise ValueError('Duplicate facial performance')
        identities.add(entry['clip'])
        performances.append(entry)
        protected.update(prepared['inputs'])
        protected[str(curve_path)] = digest(curve_path)
    if performances:
        rig["performances"] = performances
    if args.output:
        output.mkdir(parents=True,exist_ok=False)
    with (output / "facial-rig.json").open("x") as file:
        json.dump(rig, file, indent=2, allow_nan=False)
        file.write("\n")
    with (output / "facial-catalog-evidence.json").open("x") as file:
        json.dump({"passed": True, "protected": protected, "rig_sha256": digest(output / "facial-rig.json"),
            "installed": False, "scope": "Cooked, revision-bound catalog. Runtime activation remains pending."}, file, indent=2)
        file.write("\n")
    print(output / "facial-rig.json")


if __name__ == "__main__":
    main()

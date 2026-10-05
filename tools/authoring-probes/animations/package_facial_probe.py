"""Build a private facial container for independent decoding. Does not install."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info


def digest(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("work", type=Path)
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    if not work.is_relative_to(ROOT / "work/cine-animation-port"):
        raise ValueError("Use the private animation workspace")
    report = json.loads((work / "facial-stage-verify.json").read_text())
    if not report.get("complete") or len(report["results"]) != 6 or json.loads((work / "cook-exit.json").read_text())["exit_code"] != 0:
        raise ValueError("Six fresh verified and cooked meshes are required")
    inputs = dict(report["protected"])
    dependencies = json.loads((work.parent / "shared-v1/pack-with-adapters-v2/verification.json").read_text())["dependencies"]
    dependencies = {path: checksum for path, checksum in dependencies.items() if "CSS_EveStellarBlade_" in Path(path).name}
    if len(dependencies) != 2:
        raise ValueError("Expected the audited Eve container pair")
    inputs.update(dependencies)
    if any(digest(Path(path)) != checksum for path, checksum in inputs.items()):
        raise ValueError("A verified input changed")
    output = work / "probe"
    output.mkdir(exist_ok=False)
    converter = Converter(ROOT / "build/retoc-css-target/release/retoc", DEFAULT_REPAK, output)
    stage = output / "stage"
    payloads = {}
    for row in report["results"]:
        package = row["target"]
        if not package.startswith("/Game/CSS/Locomotion/Faces/SK_Face_") or ".." in package:
            raise ValueError("Unexpected facial package")
        for suffix in (".uasset", ".uexp"):
            relative = package.removeprefix("/Game/") + suffix
            source = work / "cooked/CSSAuthoring/Content" / relative
            target = stage / "MortalShell2/Content" / relative
            if suffix == ".uasset":
                info = asset_info(source.read_bytes())
                if info["package"] != package or not any(e["class"] == "SkeletalMesh" for e in info["exports"]):
                    raise ValueError("Not the expected cooked skeletal mesh")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            payloads[str(source)] = digest(source)
    container = output / "CSS_FacialReadbackProbe_P.utoc"
    converter.run(converter.retoc, "to-zen", stage, container, "--version", "UE5_6", "--no-parallel")
    converter.run(converter.retoc, "verify", container)
    mounts = output / "containers"
    converter.base_containers(GAME, mounts)
    for path in [*map(Path, dependencies), container, container.with_suffix(".ucas")]:
        (mounts / path.name).symlink_to(path)
    if any(digest(Path(path)) != checksum for path, checksum in inputs.items()):
        raise ValueError("Protected input changed while packaging")
    (output / "receipt.json").write_text(json.dumps({"protected": inputs, "cooked": payloads,
        "containers": {str(path): digest(path) for path in (container, container.with_suffix(".ucas"))},
        "installed": False}, indent=2) + "\n")
    print(output)


if __name__ == "__main__":
    main()

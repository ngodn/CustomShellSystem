"""Package the cooked shared assets and verify their export payloads through IoStore."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from css_convert import Converter, DEFAULT_REPAK

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("cooked_project", type=Path)
parser.add_argument("game", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--retoc", type=Path, required=True)
parser.add_argument("--name", default="CSS_AstralSharedAssets_P")
parser.add_argument("--manifest", type=Path)
parser.add_argument("--variants", type=Path)
args = parser.parse_args()
if not re.fullmatch(r"CSS_[A-Za-z0-9_]+_P", args.name):
    parser.error("Container name must be a CSS_ filename ending in _P")
source = (args.cooked_project / "Content/CSS/SharedAssets").resolve(strict=True)
output = args.output.resolve()
if source == output or source in output.parents:
    parser.error("Output must be outside cooked source")
files = list(source.rglob("*"))
if not files or any(p.is_file() and p.suffix not in (".uasset", ".uexp", ".ubulk", ".uptnl") for p in files):
    raise ValueError("Unexpected cooked source files")
packages = list(source.rglob("*.uasset"))
if bool(args.manifest) != bool(args.variants):
    parser.error("Supply both --manifest and --variants")
package_names = ["/Game/CSS/SharedAssets/" + str(p.relative_to(source).with_suffix("")) for p in packages]
if args.manifest:
    manifest = json.loads(args.manifest.read_text())
    variants = json.loads(args.variants.read_text())["materials"]
    parents = [row["material"].split(".")[0] for row in manifest["materials"]]
    alternates = [row["variant"].split(".")[0] for row in variants]
    if (len(parents) < 12 or len(set(parents)) != len(parents)
            or len(set(alternates)) != len(parents) or len(variants) != len(parents)
            or {row["parent"].split(".")[0] for row in variants} != set(parents)):
        raise ValueError("Companion manifest has missing or duplicate parents/variants")
    expected = set(parents + alternates + [p.split(".")[0] for p in manifest["neutral_textures"]])
    expected.add("/Game/CSS/SharedAssets/Astral/ABP_CopyPose")
    if set(package_names) != expected:
        raise ValueError("Cooked packages differ from the companion manifests")
elif len(packages) != 28:
    raise ValueError("Expected 28 baseline assets; supply manifests for an expanded package")
if any(not p.with_suffix(".uexp").is_file() for p in packages):
    raise ValueError("Cooked package has no export payload")
output.mkdir(parents=True, exist_ok=False)
legacy = output / "legacy"
shutil.copytree(source, legacy / "MortalShell2/Content/CSS/SharedAssets")
converter = Converter(args.retoc, DEFAULT_REPAK, output)
utoc = output / (args.name + ".utoc")
converter.run(converter.retoc, "to-zen", "--version", "UE5_6", legacy, utoc)
converter.run(converter.retoc, "verify", utoc)
converter.base_containers(args.game.resolve(strict=True), output / "containers")
for path in output.glob(args.name + ".*"):
    (output / "containers" / path.name).symlink_to(path)
converter.run(converter.retoc, "to-legacy", output / "containers", output / "readback",
              "--filter", "MortalShell2/Content/CSS/SharedAssets", "--no-shaders",
              "--no-script-objects", "--version", "UE5_6")
receipts = []
for path in legacy.rglob("*"):
    if not path.is_file() or path.suffix == ".uasset":
        continue
    relative = path.relative_to(legacy)
    if path.read_bytes() != (output / "readback" / relative).read_bytes():
        raise ValueError(f"Export payload changed: {relative}")
    receipts.append({"path": str(relative), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
(output / "roundtrip.json").write_text(json.dumps({"passed": True, "container": args.name, "exports": receipts,
    "installed": False}, indent=2) + "\n")
(output / "packages.txt").write_text("\n".join(sorted(package_names)) + "\n")
print(f"Verified {len(receipts)} export payloads; independent shader readback remains required")

"""Package the cooked shared assets and verify their export payloads through IoStore."""
import argparse
import hashlib
import json
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
args = parser.parse_args()
source = (args.cooked_project / "Content/CSS/SharedAssets").resolve(strict=True)
output = args.output.resolve()
if source == output or source in output.parents:
    parser.error("Output must be outside cooked source")
files = list(source.rglob("*"))
if not files or any(p.is_file() and p.suffix not in (".uasset", ".uexp", ".ubulk", ".uptnl") for p in files):
    raise ValueError("Unexpected cooked source files")
packages = list(source.rglob("*.uasset"))
if len(packages) != 28 or any(not p.with_suffix(".uexp").is_file() for p in packages):
    raise ValueError("Expected pose, twelve parents, twelve culling variants and three neutral textures")
output.mkdir(parents=True, exist_ok=False)
legacy = output / "legacy"
shutil.copytree(source, legacy / "MortalShell2/Content/CSS/SharedAssets")
converter = Converter(args.retoc, DEFAULT_REPAK, output)
utoc = output / "CSS_SharedAssets_P.utoc"
converter.run(converter.retoc, "to-zen", "--version", "UE5_6", legacy, utoc)
converter.run(converter.retoc, "verify", utoc)
converter.base_containers(args.game.resolve(strict=True), output / "containers")
for path in output.glob("CSS_SharedAssets_P.*"):
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
(output / "roundtrip.json").write_text(json.dumps({"passed": True, "exports": receipts,
    "installed": False}, indent=2) + "\n")
package_names = ["/Game/CSS/SharedAssets/" + str(p.relative_to(source).with_suffix("")) for p in packages]
(output / "packages.txt").write_text("\n".join(sorted(package_names)) + "\n")
print(f"Verified {len(receipts)} export payloads; independent shader readback remains required")

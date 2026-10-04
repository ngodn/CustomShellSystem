"""Compile the private eye reconstruction and ghost companions in UE 5.6.1."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
output = root / "native-eye-compiled.json"
if output.exists():
    raise FileExistsError(output)
report = json.loads((root / "native-eye-materials.json").read_text())


def check_sources():
    for path, digest in report["protected"].items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


check_sources()
assets = [unreal.EditorAssetLibrary.load_asset(row["path"]) for row in report["materials"]]
if not all(assets):
    raise RuntimeError("Missing eye fixture")
result = json.loads(unreal.UGMaterialLibrary.finish_materials(assets))
check_sources()
result["source_files_unchanged"] = True
output.write_text(json.dumps(result, indent=2) + "\n")
if not result["passed"]:
    raise RuntimeError(result)
unreal.log("CSS_NATIVE_EYES_COMPILED")

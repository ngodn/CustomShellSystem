"""Compile the isolated native-ghost graphs without capture/readback."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
target = root / "native-ghost-compiled.json"
if target.exists():
    raise FileExistsError(target)
report = json.loads((root / "native-ghost-material.json").read_text())
for path, digest in report["protected"].items():
    if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"Source changed: {path}")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "r.DumpShaderDebugInfo 1")
paths = [report["material"], *report["instances"]]
assets = [unreal.EditorAssetLibrary.load_asset(path) for path in paths]
if not all(assets):
    raise RuntimeError("Could not load all native ghost materials")
compiled = json.loads(unreal.UGMaterialLibrary.finish_materials(assets))
for path, digest in report["protected"].items():
    if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"Source changed during compilation: {path}")
compiled["source_files_unchanged"] = True
target.write_text(json.dumps(compiled, indent=2) + "\n")
if not compiled["passed"]:
    raise RuntimeError(f"Native ghost compilation failed: {compiled}")
unreal.log("CSS_ASTRAL_NATIVE_COMPILED")

"""Create the shared pose template and check it against isolated outfit meshes."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
report = root / "copy-pose-check.json"
package = "/Game/CSS/SharedAssets/Astral/ABP_CopyPose"
if report.exists() or unreal.EditorAssetLibrary.does_asset_exist(package):
    raise FileExistsError("This pose trial already exists")
if not unreal.SystemLibrary.get_engine_version().startswith("5.6.1-"):
    raise RuntimeError("Expected Unreal Engine 5.6.1")

protected = {}


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Cannot load fixture: {path}")
    file = root / "Content" / (path.removeprefix("/Game/") + ".uasset")
    protected[str(file.resolve())] = hashlib.sha256(file.read_bytes()).hexdigest()
    return asset


meshes = {name: load("/Game/CSS/UnholyGenessa/" + name)
          for name in ("SK_EveW3", "SK_GenessaW3", "SK_EveG")}
sequence = load("/Game/CSS/Eve/Anim/AN_S1_Walk")
blueprint = unreal.CSSPoseLibrary.create_copy_pose_template(package)
if blueprint is None or not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
    raise RuntimeError("Cannot create or save shared copy-pose template")
cases = []
for source, visual in (("SK_EveW3", "SK_EveW3"),
                       ("SK_EveG", "SK_EveW3"),
                       ("SK_EveG", "SK_GenessaW3")):
    cases.append(json.loads(unreal.CSSPoseLibrary.check_copy_pose(
        blueprint, meshes[source], meshes[visual], sequence)))
for path, expected in protected.items():
    if hashlib.sha256(Path(path).read_bytes()).hexdigest() != expected:
        raise RuntimeError(f"Source asset changed: {path}")
result = {"engine": unreal.SystemLibrary.get_engine_version(), "asset": package,
          "protected": protected, "cases": cases,
          "passed": all(case["passed"] for case in cases)}
report.write_text(json.dumps(result, indent=2) + "\n")
if not result["passed"]:
    raise RuntimeError("Copy-pose fixture failed; see copy-pose-check.json")

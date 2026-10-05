"""UE 5.6.1: copy verified Eve expression meshes to the shared namespace."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ["CSS_ANIM_WORK"]).resolve()
CONTENT = ROOT / "CSS-eins0fx-collections/tools/CSSAuthoring/Content"
MODE = os.environ.get("CSS_FACE_STAGE_MODE", "create")
if MODE not in ("create", "verify") or not WORK.is_relative_to(ROOT / "CustomShellSystem/work/cine-animation-port"):
    raise ValueError("Expected the private animation workspace")
INPUT = WORK.parent / "facial-export-v1/facial-shapes.json"
AUDIT = json.loads((WORK.parent / "facial-export-v1/ue-verification.json").read_text())
if not AUDIT.get("complete") or len(AUDIT["results"]) != 6:
    raise ValueError("Six verified facial candidates are required")


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def disk(package):
    return CONTENT / (package.removeprefix("/Game/") + ".uasset")


protected = dict(AUDIT["protected"])
protected[str(INPUT)] = digest(INPUT)
results = []
report = WORK / ("facial-stage-" + MODE + ".json")
if report.exists():
    raise FileExistsError(report)
report.write_text(json.dumps({"complete": False, "results": []}) + "\n")
if any(digest(Path(path)) != expected for path, expected in protected.items()):
    raise ValueError("Original or facial candidate changed since verification")
names = {entry["name"] for entry in json.loads(INPUT.read_text())["morph_targets"]}
for item in AUDIT["results"]:
    source_path, candidate_path = item["source"], item["candidate"]
    if not candidate_path.startswith("/Game/CSS/AnimLab/SK_Face_"):
        raise ValueError("Unexpected facial source")
    target_path = "/Game/CSS/Locomotion/Faces/" + candidate_path.rsplit("/", 1)[-1]
    if MODE == "create" and disk(target_path).exists():
        raise FileExistsError(target_path)
    if MODE == "verify":
        protected[str(disk(target_path))] = digest(disk(target_path))
    original = unreal.load_asset(source_path)
    candidate = unreal.load_asset(candidate_path)
    target = (unreal.EditorAssetLibrary.duplicate_asset(candidate_path, target_path)
              if MODE == "create" else unreal.load_asset(target_path))
    if not original or not candidate or not isinstance(target, unreal.SkeletalMesh):
        raise ValueError("Missing facial mesh")
    for source in (original, candidate):
        comparison = unreal.CSSFacialLibrary.verify_expression_shapes(source, target, str(INPUT))
        if not comparison.startswith("PASS:"):
            raise ValueError(target_path + ": " + comparison)
        if unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source) != unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target):
            raise ValueError("Shared facial bone binding differs")
        for field in ("skeleton", "physics_asset", "shadow_physics_asset", "post_process_anim_blueprint", "materials"):
            if source.get_editor_property(field) != target.get_editor_property(field):
                raise ValueError("Shared facial binding differs: " + field)
    old = {str(m.get_name()) for m in original.get_editor_property("morph_targets")}
    actual = {str(m.get_name()) for m in target.get_editor_property("morph_targets")}
    if actual != old | names or len(actual) != 121:
        raise ValueError("Shared facial morph set differs")
    if MODE == "create" and not unreal.EditorAssetLibrary.save_loaded_asset(target, False):
        raise ValueError("Could not save shared facial mesh")
    results.append({"source": source_path, "candidate": candidate_path, "target": target_path,
                    "morphs": len(actual), "sha256": digest(disk(target_path)), "comparison": comparison})
    report.write_text(json.dumps({"complete": False, "results": results}, indent=2) + "\n")
    print("SHARED_FACIAL_PASS", target_path, flush=True)
if any(digest(Path(path)) != expected for path, expected in protected.items()):
    raise ValueError("Shared facial staging changed a protected input")
report.write_text(json.dumps({"complete": True, "results": results, "protected": protected,
                             "installed": False, "scope": "Editor geometry, morphs and bindings. Cooked/live checks pending."}, indent=2) + "\n")

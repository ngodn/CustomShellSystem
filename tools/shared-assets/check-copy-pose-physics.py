"""Check the shared pose template with the outfit's existing post-process graph."""
import json
import os
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
name = os.environ.get("CSS_POSE_CHECK_NAME", "copy-pose-physics-check")
if not name or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" for c in name):
    raise ValueError("Invalid report name")
report = root / (name + ".json")
if report.exists():
    raise FileExistsError(report)
load = unreal.EditorAssetLibrary.load_asset
blueprint = load("/Game/CSS/SharedAssets/Astral/ABP_CopyPose")
source = load("/Game/CSS/UnholyGenessa/SK_EveG")
sequence = load("/Game/CSS/Eve/Anim/AN_S1_Walk")
cases = []
for name in ("SK_EveW3", "SK_GenessaW3"):
    visual = load("/Game/CSS/UnholyGenessa/" + name)
    cases.append(json.loads(unreal.CSSPoseLibrary.check_copy_pose(
        blueprint, source, visual, sequence, True)))
result = {"cases": cases, "passed": all(case["passed"] for case in cases)}
report.write_text(json.dumps(result, indent=2) + "\n")
if not result["passed"]:
    raise RuntimeError("Copy-pose post-process check failed; see report")

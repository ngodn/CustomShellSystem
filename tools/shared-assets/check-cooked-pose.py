"""Validate independent AssetReadback output for the cooked shared pose template."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("readback", type=Path)
parser.add_argument("report", type=Path)
args = parser.parse_args()
if args.report.exists():
    raise FileExistsError(args.report)
package = "/Game/CSS/SharedAssets/Astral/ABP_CopyPose"
data = json.loads(args.readback.read_text())
if set(data) != {package}:
    raise ValueError("Expected exactly the shared pose package")
exports = data[package]
by_name = {row["Name"]: row for row in exports}
if len(by_name) != 4 or len(exports) != 4:
    raise ValueError("Unexpected cooked export count")
generated = by_name["ABP_CopyPose_C"]
if (generated["Type"] != "AnimBlueprintGeneratedClass" or not generated["bCooked"]
        or generated["SuperStruct"] != {"ObjectName": "Class'AnimInstance'", "ObjectPath": "/Script/Engine"}
        or generated["Properties"].get("TargetSkeleton") is not None):
    raise ValueError("Unexpected generated class parent or skeleton")
cdo = by_name["Default__ABP_CopyPose_C"]["Properties"]
expected = {"SourceMeshComponent": None, "bUseAttachedParent": True,
            "bCopyCurves": True, "bCopyCustomAttributes": True,
            "bUseMeshPose": False, "RootBoneToCopy": "None"}
if cdo["AnimGraphNode_CopyPoseFromMesh"] != expected or not cdo["bUsingCopyPoseFromMesh"]:
    raise ValueError("Cooked copy-pose defaults differ")
if cdo["AnimGraphNode_Root"]["Result"]["LinkID"] != 1:
    raise ValueError("Root is not connected to the copy node")
if [row["NodeIndex"] for row in generated["Properties"]["AnimNodeData"]] != [0, 1]:
    raise ValueError("Unexpected animation node layout")


def inspect(value):
    if isinstance(value, dict):
        for item in value.values():
            inspect(item)
    elif isinstance(value, list):
        for item in value:
            inspect(item)
    elif isinstance(value, str):
        if "CSSSharedAuthoring" in value:
            raise ValueError("Cooked class references the editor authoring module")
        if "/Game/" in value and package not in value:
            raise ValueError(f"Unexpected game-asset dependency: {value}")


inspect(exports)
args.report.write_text(json.dumps({"passed": True, "package": package,
    "export_count": len(exports), "copy_pose": expected,
    "scope": "Independent cooked class/property readback. No live game execution."}, indent=2) + "\n")

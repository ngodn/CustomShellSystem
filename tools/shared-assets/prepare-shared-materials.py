"""Stage verified candidate graphs as read-only links in the shared editor project."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("candidates", type=Path)
parser.add_argument("shared_project", type=Path)
args = parser.parse_args()
source = args.candidates.resolve(strict=True)
destination = args.shared_project.resolve(strict=True)
report = destination / "shared-material-plan.json"
if report.exists() or source == destination or not (destination / "CSSShared.uproject").is_file():
    raise ValueError("Expected separate shared editor project with no existing plan")
plan = {"materials": [], "protected": {}}
for name in ("ghost-filtered.json", "native-eye-materials.json", "native-body-materials.json",
             "native-refraction-material.json"):
    data = json.loads((source / name).read_text())
    if not data["source_files_unchanged"]:
        raise ValueError(f"Source protection failed: {name}")
    for path, digest in data["protected"].items():
        if path in plan["protected"] and plan["protected"][path] != digest:
            raise ValueError(f"Conflicting source hash: {path}")
        plan["protected"][path] = digest
    if name == "ghost-filtered.json":
        if data["composition"] != "native_filter":
            raise ValueError("Rejected material composition")
        for row in data["materials"]:
            plan["materials"].append({"candidate": row["target"], "name": row["target"].rsplit("/", 1)[1],
                                      "source_parent": row["source"]})
    elif name == "native-eye-materials.json":
        for row in data["materials"]:
            if row["role"] == "ghost":
                plan["materials"].append({"candidate": row["path"], "name": "M_Genessa_" + row["kind"]})
    elif name == "native-body-materials.json":
        for row in data["materials"]:
            plan["materials"].append({"candidate": row["path"], "name": "M_Uber_" + row["kind"],
                                      "required_switches": data["required_switches"]})
    else:
        plan["materials"].append({"candidate": data["material"], "name": "M_EyeRefraction",
                                  "scene_parameter": data["scene_parameter"]})
if len(plan["materials"]) != 12 or len({row["name"] for row in plan["materials"]}) != 12:
    raise ValueError("Unexpected material set")
for row in plan["materials"]:
    package = row["candidate"].split(".")[0]
    if not package.startswith("/Game/CSS/UnholyGenessa/"):
        raise ValueError("Unexpected candidate namespace")
    asset = source / "Content" / (package.removeprefix("/Game/") + ".uasset")
    plan["protected"][str(asset)] = hashlib.sha256(asset.read_bytes()).hexdigest()
for path, digest in plan["protected"].items():
    if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
        raise ValueError(f"Protected source changed: {path}")
links = []
for path in (source / "Content").rglob("*"):
    if not path.is_file():
        continue
    target = destination / "Content" / path.relative_to(source / "Content")
    if target.exists():
        if target.resolve() != path.resolve() and hashlib.sha256(target.read_bytes()).digest() != hashlib.sha256(path.read_bytes()).digest():
            raise ValueError(f"Conflicting staged asset: {target}")
    elif target.is_symlink():
        raise ValueError(f"Broken staged link: {target}")
    else:
        links.append((target, path.resolve(strict=True)))
for target, path in links:
    target.parent.mkdir(parents=True, exist_ok=True)
    target.symlink_to(path)
with report.open("x") as stream:
    stream.write(json.dumps(plan, indent=2) + "\n")
print(f"Prepared twelve companions and {len(links)} source links")

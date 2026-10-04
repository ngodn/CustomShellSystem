"""Resolve skeletal material slots through independently decoded cooked parents."""
import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("meshes", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("materials", type=Path, nargs="+")
    args = parser.parse_args()
    assets = {}
    for path in args.materials:
        for exports in json.loads(path.read_text()).values():
            for asset in exports:
                if asset.get("Type") in ("Material", "MaterialInstanceConstant"):
                    key = asset["Package"]
                    if key in assets and assets[key] != asset:
                        raise ValueError(f"Conflicting material: {key}")
                    assets[key] = asset

    def resolve(reference):
        path = reference["ObjectPath"].split(".")[0]
        chain = []
        while path:
            if path in chain:
                raise ValueError(f"Material parent cycle: {path}")
            chain.append(path)
            asset = assets[path]
            parent = asset.get("Properties", {}).get("Parent")
            path = parent["ObjectPath"].split(".")[0] if parent else None
        if assets[chain[-1]]["Type"] != "Material":
            raise ValueError(f"Missing root graph for {chain[0]}")
        return chain

    meshes = {}
    for exports in json.loads(args.meshes.read_text()).values():
        for mesh in exports:
            if mesh.get("Type") != "SkeletalMesh":
                continue
            slots = []
            for index, slot in enumerate(mesh["SkeletalMaterials"]):
                row = {"slot": index, "name": slot["MaterialSlotName"]}
                for field in ("Material", "OverlayMaterial"):
                    reference = slot.get(field)
                    row[field] = resolve(reference) if reference else None
                slots.append(row)
            meshes[mesh["Package"]] = slots
    if not meshes:
        raise ValueError("No skeletal meshes decoded")
    used = {path for slots in meshes.values() for slot in slots
            for field in ("Material", "OverlayMaterial") for path in slot[field] or []}
    report = {"meshes": meshes, "materials": {
        path: {"type": assets[path]["Type"],
               "properties": assets[path].get("Properties", {})}
        for path in sorted(used)},
        "limitations": ["Authored slots only; live MID overrides and hidden sections need runtime capture.",
                        "Root graph identity alone does not establish compiled instance compatibility."]}
    with args.output.open("x") as stream:
        stream.write(json.dumps(report, indent=2) + "\n")
    print(f"Resolved {len(meshes)} meshes and {len(used)} material interfaces")


if __name__ == "__main__":
    main()

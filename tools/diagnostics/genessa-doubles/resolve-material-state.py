"""Resolve UE 5.6.1 material inheritance from cooked readbacks, without loading UE.

This inventories adapter inputs. It does not reconstruct shader expressions or
prove that a companion can render a resolved permutation.
"""
import argparse
import hashlib
import json
from pathlib import Path


# Material.cpp:SetInitialValues and MaterialShared.cpp's override constructor
# use slightly different clip defaults. A disabled override never uses its value.
BASE_DEFAULTS = {"BlendMode": "BLEND_Opaque", "TwoSided": False,
                 "OpacityMaskClipValue": 0.3333, "ShadingModel": "MSM_DefaultLit"}
OVERRIDE_DEFAULTS = {**BASE_DEFAULTS, "OpacityMaskClipValue": 0.333333}
# EMaterialParameterType in UE 5.6.1 MaterialTypes.h.
PARAMETERS = {"scalar": (0, "ScalarValues", "ScalarParameterValues"),
              "vector": (1, "VectorValues", "VectorParameterValues"),
              "texture": (3, "TextureValues", "TextureParameterValues"),
              "switch": (8, "StaticSwitchValues", "StaticSwitchParameters")}


def enum(value):
    value = value.rsplit("::", 1)[-1]
    return "BLEND_Translucent" if value == "BLEND_TranslucentGreyTransmittance" else value


def parameter_key(info):
    name = info["Name"]
    if not isinstance(name, str) or not name:
        raise ValueError("Parameter name is missing")
    return name, enum(info.get("Association", "GlobalParameter")), info.get("Index", -1)


def normalize(value):
    if isinstance(value, str):
        return enum(value)
    return value


def load_assets(paths):
    assets = {}
    for path in paths:
        for exports in json.loads(path.read_text()).values():
            for asset in exports:
                if asset.get("Type") not in ("Material", "MaterialInstanceConstant"):
                    continue
                key = asset["Package"]
                retained = {field: asset.get(field, {}) for field in
                            ("Type", "Properties", "CachedExpressionData")}
                if key in assets and assets[key] != retained:
                    raise ValueError(f"Conflicting material readbacks: {key}")
                assets[key] = retained
    return assets


def resolve(assets, path):
    chain = []
    current = path
    while current:
        if current in chain or len(chain) >= 64:
            raise ValueError(f"Invalid material parent chain: {path}")
        chain.append(current)
        asset = assets[current]
        parent = asset["Properties"].get("Parent")
        current = parent["ObjectPath"].split(".")[0] if parent else None
    root = assets[chain[-1]]
    if root["Type"] != "Material":
        raise ValueError(f"Missing material root: {path}")
    properties = root["Properties"]
    base = {key: {"value": normalize(properties.get(key, default)), "source": chain[-1]}
            for key, default in BASE_DEFAULTS.items()}
    parameters = {kind: {} for kind in PARAMETERS}
    cached = root["CachedExpressionData"]
    if not cached:
        raise ValueError(f"Root parameter defaults unavailable: {chain[-1]}")
    for kind, (index, values_key, _) in PARAMETERS.items():
        entry = "RuntimeEntries" + (f"[{index}]" if index else "")
        infos = cached[entry]["ParameterInfoSet"]
        values = cached[values_key]
        if len(infos) != len(values):
            raise ValueError(f"Mismatched {kind} defaults: {chain[-1]}")
        for info, value in zip(infos, values, strict=True):
            key = parameter_key(info)
            if key in parameters[kind]:
                raise ValueError(f"Duplicate root parameter: {key}")
            parameters[kind][key] = {"value": value, "source": chain[-1]}
    layers = {"value": cached.get("MaterialLayers", {}), "source": chain[-1]}
    other_overrides = []
    for ancestor in reversed(chain[:-1]):
        properties = assets[ancestor]["Properties"]
        overrides = properties.get("BasePropertyOverrides", {})
        for key, default in OVERRIDE_DEFAULTS.items():
            if overrides.get("bOverride_" + key, False):
                value = normalize(overrides.get(key, default))
                # UE cannot replace an inherited shading model with this sentinel.
                if key == "ShadingModel" and value == "MSM_FromMaterialExpression":
                    continue
                base[key] = {"value": value, "source": ancestor}
        for key, enabled in overrides.items():
            if key.startswith("bOverride_") and enabled and key[10:] not in BASE_DEFAULTS:
                other_overrides.append({"source": ancestor, "flag": key, "properties": overrides})
        static = properties.get("StaticParametersRuntime", {})
        if static.get("bHasMaterialLayers", False):
            layers = {"value": static["MaterialLayers"], "source": ancestor}
        for kind, (_, _, key) in PARAMETERS.items():
            rows = static.get(key, []) if kind == "switch" else properties.get(key, [])
            seen = set()
            for row in rows:
                identity = parameter_key(row["ParameterInfo"])
                if identity in seen:
                    raise ValueError(f"Ambiguous {kind} override: {ancestor}: {identity}")
                seen.add(identity)
                if kind == "switch" and not row.get("bOverride", False):
                    continue
                value = row.get("Value", False) if kind == "switch" else row["ParameterValue"]
                parameters[kind][identity] = {"value": value, "source": ancestor}
    return {"chain": chain, "base": base, "layers": layers,
            "other_base_overrides": other_overrides,
            "parameters": {kind: [{"name": key[0], "association": key[1], "index": key[2], **row}
                                   for key, row in sorted(values.items())]
                           for kind, values in parameters.items()}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("readbacks", type=Path, nargs="+")
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inventory = json.loads(args.inventory.read_text())
    assets = load_assets(args.readbacks)
    paths = {chain[0] for slots in inventory["meshes"].values() for slot in slots
             for field in ("Material", "OverlayMaterial") if (chain := slot[field])}
    materials = {path: resolve(assets, path) for path in sorted(paths)}
    counts = {}
    for slots in inventory["meshes"].values():
        for slot in slots:
            for field in ("Material", "OverlayMaterial"):
                if not slot[field]:
                    continue
                material = materials[slot[field][0]]
                mode = material["base"]["BlendMode"]["value"]
                key = material["chain"][-1] + "|" + mode
                counts[key] = counts.get(key, 0) + 1
    report = {"engine": "5.6.1", "materials": materials, "slot_counts": counts,
              "sources": {str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in [args.inventory, *args.readbacks]},
              "limitations": ["Cooked defaults only. Live MID values must be copied at summon time.",
                              "Texture values retain readback representation; null override behavior is not inferred.",
                              "Material-layer structures are recorded, not flattened or reconstructed.",
                              "No shader formula, render compatibility or adapter support is established."]}
    with args.output.open("x") as stream:
        stream.write(json.dumps(report, indent=2, allow_nan=False) + "\n")
    print(json.dumps({"materials": len(materials), "slot_counts": counts}, indent=2))


if __name__ == "__main__":
    main()

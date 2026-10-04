"""Verify traversal and sidearm definitions from AssetReadback JSON (Python 3.14)."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("evidence", type=Path)
args = parser.parse_args()
packages = {}
for name in ("traversal.json", "controller.json", "aim.json", "remaining-weapons.json"):
    packages.update(json.loads((args.evidence / name).read_text()))


def exports(leaf):
    matches = [values for path, values in packages.items() if path.endswith(f"/{leaf}.uasset")]
    if len(matches) != 1:
        raise ValueError(f"Expected exactly one package for {leaf}")
    return matches[0]


def export(leaf, name):
    return next(value for value in exports(leaf) if value["Name"] == name)


def calls(value):
    if isinstance(value, dict):
        if value.get("Token", "").endswith("Function") or value.get("Token") == "EX_CallMath":
            function = value["Function"]
            yield function["ObjectName"] if isinstance(function, dict) else function
        for child in value.values():
            yield from calls(child)
    elif isinstance(value, list):
        for child in value:
            yield from calls(child)


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


switch = list(calls(export("BP_PlayerController", "SwitchCharacterMesh")["ScriptBytecode"]))
save = "Class'CSAnimationBlueprintLibrary:SaveAnimationInstanceState'"
restore = "Class'CSAnimationBlueprintLibrary:LoadAnimationInstanceState'"
require(switch.index(save) < switch.index("SetSkinnedAssetAndUpdate") < switch.index(restore),
        "Controller mesh transition no longer matches the inspected sequence")
require(not any("LinkAnimClassLayers" in name or "ApplyAnimationLayers" in name for name in switch),
        "Controller now explicitly restores animation layers; reassess the repair")
for ability in ("GA_Traversal_BoneGate_Far", "GA_ShellTraversalBase"):
    for method in ("SwitchToDarkFormMesh", "SwitchToShellMesh"):
        require(method in calls(export(ability, method)["ScriptBytecode"]),
                f"{ability}.{method} no longer calls the controller transition")
for leaf in ("GA_Traversal_ShellThrow_High", "GA_Traversal_ShellThrow_Long"):
    parent = export(leaf, leaf + "_C")["Super"]["ObjectName"]
    require("GA_Traversal_ShellThrow_C" in parent, f"{leaf} inheritance changed")

aims = {}
for weapon in ("Ballistazooka", "Crossbow", "CursedChild", "MachineGun", "NailShotgun", "ParasiteGun", "Trebuchaxe"):
    leaf = "WP_" + weapon
    layers = export(leaf, "Default__" + leaf + "_C")["Properties"]["OnEquip_AnimationLayers"]
    require(len(layers) == 1, f"Review {weapon}'s equip layer list")
    aim = layers[0]["AssetPathName"].rsplit(".", 1)[1]
    definition = export(aim.removesuffix("_C"), aim)
    parent = definition.get("SuperStruct", definition.get("Super"))["ObjectName"]
    require(parent == "AnimBlueprintGeneratedClass'ABPL_Aim_Default_C'", f"Review {aim}'s parent")
    aims[weapon] = aim
print("PASS: controller mesh-swap sequence, both traversal families, and seven equipped aiming layers")
print(json.dumps(aims, indent=2))

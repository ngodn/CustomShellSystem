#!/usr/bin/env python3
"""Generate catalog metadata from retained cooked exports, without enabling moves."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def collect_bindings(selectors: dict, abilities: dict) -> tuple[dict[str, set[str]], list[str]]:
    bindings: dict[str, set[str]] = {}
    unresolved: set[str] = set()
    for selector, row in sorted(selectors.items()):
        tags = set(row.get("tags") or [])
        light = "Ability.Attack.Selector.Light" in tags
        heavy = "Ability.Attack.Selector.Heavy" in tags
        if light == heavy:
            unresolved.add(selector)
            continue
        prefix, kind = ("L", "Light") if light else ("H", "Heavy")
        for i, ability in enumerate(row.get("combo") or []):
            evidence = abilities.get(ability) if isinstance(ability, str) else None
            hold = evidence.get("is_hold") if evidence else None
            if not isinstance(hold, bool) or i >= 3:
                if isinstance(ability, str): unresolved.add(ability)
                continue
            bindings.setdefault(ability, set()).add(prefix + "C" if hold else prefix + str(i + 1))
        for ability in row.get("additional") or []:
            evidence = abilities.get(ability) if isinstance(ability, str) else None
            ability_tags = set(evidence.get("ability_tags") or []) if evidence else set()
            if "Ability.Attack.Melee.Finisher." + kind in ability_tags:
                bindings.setdefault(ability, set()).add(prefix + "F")
            elif isinstance(ability, str):
                unresolved.add(ability)
    return bindings, sorted(unresolved)


def ability_classes(listing: Path):
    """Loadable class path per ability name: the exported ability list gives packages under
    /Game/Sparta/Core/Player/Ability/; Smert's attacks live under /Game/Sparta/Core/Characters/Player/Smert/Abilities/."""
    packages = {Path(line).stem: "/Game/Sparta/Core/Player/Ability/" + line.strip().removesuffix(".uasset")
                for line in listing.read_text().splitlines() if line.strip()}
    def resolve(ability: str) -> str:
        name = ability.removesuffix("_C")
        package = packages.get(name)
        if not package and name.startswith("GA_Player_Attack_Smert_"):
            package = "/Game/Sparta/Core/Characters/Player/Smert/Abilities/" + name
        return f"{package}.{name}_C" if package else ""
    return resolve


def generate() -> dict:
    sources = {
        "abilities": ROOT / "work/movesets/abilities.jsonl",
        "selectors": ROOT / "work/movesets/selectors.json",
        "montages": ROOT / "work/movesets/montages.jsonl",
        "items": ROOT / "work/damage-pipeline/other_classes.txt",
        "ability_packages": ROOT / "work/movesets/list-player-abilities.txt",
    }
    ability_class = ability_classes(sources["ability_packages"])
    abilities = {r["class"]: (i, r) for i, line in enumerate(sources["abilities"].read_text().splitlines(), 1)
                 for r in [json.loads(line)]}
    montages = {r["path"]: r for line in sources["montages"].read_text().splitlines() for r in [json.loads(line)]}
    selectors = json.loads(sources["selectors"].read_text())
    bindings, unresolved_roles = collect_bindings(selectors, {name: row for name, (_, row) in abilities.items()})
    moves = []
    missing = list(unresolved_roles)
    for ability, slots in sorted(bindings.items()):
        if ability not in abilities:
            missing.append(ability)
            continue
        line, row = abilities[ability]
        path = row.get("montage_path")
        montage = montages.get(path)
        if not path or not montage:
            missing.append(ability)
            continue
        moves.append({
            "id": ability, "display_name": ability.removeprefix("GA_Player_").removesuffix("_C"),
            "ability": ability, "montage": path + "." + path.rsplit("/", 1)[1],
            "slots": sorted(slots), "skeleton": montage.get("skeleton"),
            "source_name": path.split("/Attacks/", 1)[-1].split("/", 1)[0],
            "payload": row.get("payload"), "hits": row.get("hits", []),
            "sections": montage.get("sections", []), "tracks": montage.get("tracks", []),
            "windows": row.get("windows", {}), "play_rate": row.get("play_rate"),
            "runtime_verified": False,
            "evidence": {"ability_line": line, "montage_export": montage["source_json"]},
            "ability_class": ability_class(ability),
        })
    items = []
    current = None
    for i, line in enumerate(sources["items"].read_text().splitlines(), 1):
        if line and not line.startswith(" "):
            current = line.split(" <", 1)[0]
        if "[sub SpartaItemFragment_StatLevels" not in line or "UpgradeStat.Melee.Attack.Finisher." not in line:
            continue
        fragment = json.loads(line[line.index("{"):])
        stats = fragment.get("UpgradeLevelsMap", [])
        for stat in stats:
            key = stat["Key"]
            if not key.startswith("UpgradeStat.Melee.Attack.Finisher."):
                continue
            slot = "LF" if ".Light." in key else "HF" if ".Heavy." in key else None
            if slot:
                exports = sorted(ROOT.glob(f"work/damage-pipeline/export*/MortalShell2_Content_Sparta_Core_Tarstones_Melee_{current}.uasset.json"))
                if len(exports) != 1:
                    raise ValueError(f"Ambiguous or missing item export: {current}")
                exported = json.loads(exports[0].read_text())
                display = next(e["Properties"] for e in exported if e["Type"] == "ItemFragment_Display")
                compatibility = next(e["Properties"] for e in exported if e["Type"] == "ItemFragment_Tarstone")
                sources[current] = exports[0]
                items.append({"id": current, "slot": slot, "stat": key,
                              "levels": stat["Value"]["FloatArray"], "evidence_line": i,
                              "display_name": display["DisplayName"]["LocalizedString"],
                              "description": display["Description"]["LocalizedString"],
                              "icon": display["Icon"]["AssetPathName"],
                              "compatibility_tags": compatibility.get("CompatibilityTags", []),
                              "runtime_verified": False})
    return {"schema_version": 1, "game_build": "MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241",
            "sources": {k: {"path": str(p.relative_to(ROOT)), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                        for k, p in sources.items()},
            "moves": moves, "tarstone_stats": items, "unresolved_abilities": missing}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output = ROOT / "data/catalog.json"
    text = json.dumps(generate(), indent=2, ensure_ascii=False) + "\n"
    if args.check:
        if not output.is_file() or output.read_text() != text:
            raise SystemExit("Catalog differs from retained sources; regenerate it")
    else:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text)
    print(f"Catalog {'verified' if args.check else 'generated'}: {output}")


if __name__ == "__main__":
    main()

"""Validate the core source before authoring. The C++ parser applies the same schema."""
import json
import math
from pathlib import Path


def validate(value, spec, path):
    kind = spec["type"]
    if kind == "object":
        if not isinstance(value, dict) or value.keys() != spec["fields"].keys():
            raise ValueError(f"{path}: missing or unknown fields")
        for key, child in spec["fields"].items():
            validate(value[key], child, path + "." + key)
    elif kind == "array":
        if not isinstance(value, list) or not spec["min"] <= len(value) <= spec["max"]:
            raise ValueError(f"{path}: invalid array length")
        for item in value:
            validate(item, spec["items"], path + "[]")
    elif kind in ("integer", "number"):
        valid_type = type(value) is int if kind == "integer" else type(value) in (int, float)
        if not valid_type or not math.isfinite(value) or not spec["min"] <= value <= spec["max"]:
            raise ValueError(f"{path}: invalid number")
    elif kind == "string":
        if not isinstance(value, str) or "\0" in value or not spec["min"] <= len(value.encode("utf-8")) <= spec["max"]:
            raise ValueError(f"{path}: invalid text")
    elif kind == "boolean" and type(value) is not bool:
        raise ValueError(f"{path}: expected boolean")


def load_rows():
    here = Path(__file__).resolve().parent
    rows = json.loads((here / "content.json").read_text(encoding="utf-8"))
    schema = json.loads((here / "schema.json").read_text(encoding="utf-8"))
    if rows.keys() != schema.keys():
        raise ValueError("Core row set differs from schema")
    for name, row in rows.items():
        validate(row, schema[name], name)
    if rows["style.materials"]["root"] != "/Game/CSSX/CINE":
        raise ValueError("Core package root is fixed by the loader ABI")
    return rows


if __name__ == "__main__":
    print(f"Validated {len(load_rows())} core rows")

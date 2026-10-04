"""Record coverage wiring without changing staged or source materials."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EDIT = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
report = json.loads((ROOT / "ghost-coverage.json").read_text())


def expression(material, node, depth=0):
    if node is None or depth > 2:
        return None
    result = {"class": node.get_class().get_name(), "path": node.get_path_name()}
    if isinstance(node, unreal.MaterialExpressionTextureSampleParameter2D):
        result["parameter"] = str(node.get_editor_property("parameter_name"))
    result["inputs"] = [{"output": EDIT.get_input_node_output_name_for_material_expression(node, source),
                         "node": expression(material, source, depth + 1)}
                        for source in EDIT.get_inputs_for_material_expression(material, node) if source]
    return result


rows = []
for row in report["materials"]:
    for kind in ("source", "target"):
        material = unreal.load_asset(row[kind])
        node = EDIT.get_material_property_input_node(material, MP.MP_OPACITY)
        rows.append({"kind": kind, "material": row[kind],
                     "output": EDIT.get_material_property_input_node_output_name(material, MP.MP_OPACITY),
                     "expression": expression(material, node)})
with (ROOT / "ghost-coverage-wiring.json").open("x") as stream:
    stream.write(json.dumps(rows, indent=2) + "\n")
unreal.log("CSS_GHOST_COVERAGE_INSPECTED")

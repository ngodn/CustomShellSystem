"""Inheritance failures that would select the wrong shared ghost shader."""
import importlib.util
from pathlib import Path
import unittest


script = Path(__file__).resolve().parents[1] / "tools/diagnostics/genessa-doubles/resolve-material-state.py"
spec = importlib.util.spec_from_file_location("material_state", script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def info(name, association="GlobalParameter", index=-1):
    return {"Name": name, "Association": association, "Index": index}


def fixtures():
    cached = {}
    for _, (index, values, _) in module.PARAMETERS.items():
        cached["RuntimeEntries" + (f"[{index}]" if index else "")] = {"ParameterInfoSet": []}
        cached[values] = []
    cached["RuntimeEntries"]["ParameterInfoSet"] = [info("Opacity Strength")]
    cached["ScalarValues"] = [1.0]
    cached["RuntimeEntries[8]"]["ParameterInfoSet"] = [info("USE OPACITY")]
    cached["StaticSwitchValues"] = [True]
    return {"/Root": {"Type": "Material", "Properties": {}, "CachedExpressionData": cached},
            "/Parent": {"Type": "MaterialInstanceConstant", "Properties": {
                "Parent": {"ObjectPath": "/Root.0"}}, "CachedExpressionData": {}},
            "/Child": {"Type": "MaterialInstanceConstant", "Properties": {
                "Parent": {"ObjectPath": "/Parent.0"}}, "CachedExpressionData": {}}}


class InheritanceTests(unittest.TestCase):
    def test_inactive_stored_base_values_do_not_override_parent(self):
        assets = fixtures()
        assets["/Parent"]["Properties"]["BasePropertyOverrides"] = {
            "bOverride_BlendMode": True, "BlendMode": "BLEND_Masked",
            "bOverride_TwoSided": True, "TwoSided": True,
            "bOverride_OpacityMaskClipValue": True, "OpacityMaskClipValue": .6}
        assets["/Child"]["Properties"]["BasePropertyOverrides"] = {
            "BlendMode": "BLEND_Opaque", "TwoSided": False, "OpacityMaskClipValue": .3333}
        result = module.resolve(assets, "/Child")["base"]
        self.assertEqual(result["BlendMode"], {"value": "BLEND_Masked", "source": "/Parent"})
        self.assertEqual(result["OpacityMaskClipValue"]["value"], .6)
        self.assertTrue(result["TwoSided"]["value"])

    def test_enabled_omitted_value_uses_struct_default(self):
        assets = fixtures()
        assets["/Root"]["Properties"]["BlendMode"] = "BLEND_Masked"
        assets["/Child"]["Properties"]["BasePropertyOverrides"] = {"bOverride_BlendMode": True}
        result = module.resolve(assets, "/Child")["base"]
        self.assertEqual(result["BlendMode"]["value"], "BLEND_Opaque")
        self.assertEqual(result["OpacityMaskClipValue"]["value"], .3333)

    def test_static_false_override_and_inactive_descendant(self):
        assets = fixtures()
        assets["/Parent"]["Properties"]["StaticParametersRuntime"] = {"StaticSwitchParameters": [
            {"ParameterInfo": info("USE OPACITY"), "bOverride": True, "Value": False}]}
        assets["/Child"]["Properties"]["StaticParametersRuntime"] = {"StaticSwitchParameters": [
            {"ParameterInfo": info("USE OPACITY"), "Value": True}]}
        switches = module.resolve(assets, "/Child")["parameters"]["switch"]
        self.assertEqual(switches[0]["value"], False)
        self.assertEqual(switches[0]["source"], "/Parent")

    def test_zero_override_and_layer_identity_are_preserved(self):
        assets = fixtures()
        assets["/Child"]["Properties"]["ScalarParameterValues"] = [
            {"ParameterInfo": info("Opacity Strength"), "ParameterValue": 0.0},
            {"ParameterInfo": info("Opacity Strength", "LayerParameter", 0), "ParameterValue": .8}]
        values = module.resolve(assets, "/Child")["parameters"]["scalar"]
        self.assertEqual([(row["association"], row["value"]) for row in values],
                         [("GlobalParameter", 0.0), ("LayerParameter", .8)])

    def test_shading_expression_sentinel_keeps_parent(self):
        assets = fixtures()
        assets["/Child"]["Properties"]["BasePropertyOverrides"] = {
            "bOverride_ShadingModel": True, "ShadingModel": "EMaterialShadingModel::MSM_FromMaterialExpression"}
        self.assertEqual(module.resolve(assets, "/Child")["base"]["ShadingModel"]["value"], "MSM_DefaultLit")

    def test_translucent_alias(self):
        assets = fixtures()
        assets["/Root"]["Properties"]["BlendMode"] = "EBlendMode::BLEND_TranslucentGreyTransmittance"
        self.assertEqual(module.resolve(assets, "/Child")["base"]["BlendMode"]["value"], "BLEND_Translucent")

    def test_cycle_and_missing_defaults_are_rejected(self):
        assets = fixtures()
        assets["/Parent"]["Properties"]["Parent"]["ObjectPath"] = "/Child.0"
        with self.assertRaises(ValueError):
            module.resolve(assets, "/Child")
        assets = fixtures()
        assets["/Root"]["CachedExpressionData"]["ScalarValues"] = []
        with self.assertRaises(ValueError):
            module.resolve(assets, "/Child")


if __name__ == "__main__":
    unittest.main()

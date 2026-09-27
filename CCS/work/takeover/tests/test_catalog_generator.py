import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("catalog_generator", Path(__file__).parents[1] / "generate_catalog.py")
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class CatalogBindingsTests(unittest.TestCase):
    def test_names_do_not_determine_roles(self):
        selectors = {"renamed_selector": {
            "tags": ["Ability.Attack.Selector.Light"],
            "combo": ["normal_Hold_C", "renamed_charged"],
            "additional": ["guessed_Finisher_C", "renamed_finisher"],
        }}
        abilities = {
            "normal_Hold_C": {"is_hold": False},
            "renamed_charged": {"is_hold": True},
            "guessed_Finisher_C": {"ability_tags": []},
            "renamed_finisher": {"ability_tags": ["Ability.Attack.Melee.Finisher.Light"]},
        }
        bindings, unresolved = generator.collect_bindings(selectors, abilities)
        self.assertEqual(bindings, {"normal_Hold_C": {"L1"}, "renamed_charged": {"LC"},
                                    "renamed_finisher": {"LF"}})
        self.assertEqual(unresolved, ["guessed_Finisher_C"])

    def test_missing_and_conflicting_evidence_stays_unresolved(self):
        selectors = {
            "ambiguous": {"tags": ["Ability.Attack.Selector.Light", "Ability.Attack.Selector.Heavy"],
                          "combo": ["unclassified"]},
            "heavy": {"tags": ["Ability.Attack.Selector.Heavy"],
                      "combo": ["missing", "bad_bool"], "additional": ["wrong_kind"]},
        }
        abilities = {"bad_bool": {"is_hold": "false"},
                     "wrong_kind": {"ability_tags": ["Ability.Attack.Melee.Finisher.Light"]}}
        bindings, unresolved = generator.collect_bindings(selectors, abilities)
        self.assertFalse(bindings)
        self.assertEqual(unresolved, ["ambiguous", "bad_bool", "missing", "wrong_kind"])

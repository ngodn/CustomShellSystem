import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


class ToolTests(unittest.TestCase):
    def test_build_resets_cached_diagnostic_options(self):
        tool = module(ROOT / "tools/ccs.py", "ccs_tool")
        with tempfile.TemporaryDirectory(dir=ROOT / "work/takeover") as temporary:
            task_root = Path(temporary)
            out = task_root / "build/windows"
            out.mkdir(parents=True)
            for name in ("main.dll", "ccs_core.dll"):
                (out / name).touch()
            with patch.object(tool, "ROOT", task_root), patch.object(tool.subprocess, "run") as run:
                self.assertEqual(tool.build(), out)
                configure = run.call_args_list[0].args[0]
                self.assertIn("-DCCS_FRAME_PROFILE=OFF", configure)
                self.assertIn("-DCCS_EXPERIMENTAL_MENU=OFF", configure)
                self.assertIn("-DCCS_DISCOVERY_PROBE=OFF", configure)
                self.assertIn("-DCCS_ATTACK_PROBE=OFF", configure)

    def test_build_rejects_combining_menu_and_probe(self):
        tool = module(ROOT / "tools/ccs.py", "ccs_tool")
        with patch.object(tool.subprocess, "run") as run:
            with self.assertRaisesRegex(ValueError, "separate builds"):
                tool.build(menu=True, registry=True, profile=True)
            run.assert_not_called()

    def test_attack_build_is_separate_and_profiled(self):
        tool = module(ROOT / "tools/ccs.py", "ccs_tool")
        with patch.object(tool.subprocess, "run") as run:
            for options in ({"menu": True}, {"probe": True}, {"registry": True}):
                with self.assertRaisesRegex(ValueError, "separate builds"):
                    tool.build(attack=True, **options)
            run.assert_not_called()
        with tempfile.TemporaryDirectory(dir=ROOT / "work/takeover") as temporary:
            task_root = Path(temporary)
            out = task_root / "build/windows-attack-profile"
            out.mkdir(parents=True)
            for name in ("main.dll", "ccs_core.dll"):
                (out / name).touch()
            with patch.object(tool, "ROOT", task_root), patch.object(tool.subprocess, "run") as run:
                self.assertEqual(tool.build(attack=True, profile=True), out)
                configure = run.call_args_list[0].args[0]
                self.assertIn("-DCCS_ATTACK_PROBE=ON", configure)
                self.assertIn("-DCCS_FRAME_PROFILE=ON", configure)
                self.assertIn("-DCCS_EXPERIMENTAL_MENU=OFF", configure)
                self.assertIn("-DCCS_DISCOVERY_PROBE=OFF", configure)

    def test_cli_rejects_ignored_or_conflicting_variant_flags(self):
        tool = module(ROOT / "tools/ccs.py", "ccs_tool")
        for args in (("build", "--attack"), ("status", "--registry"),
                     ("stage", "--attack", "--probe"), ("build-profile", "--attack", "--menu")):
            with patch("sys.argv", ["ccs.py", *args]), patch.object(tool, "build") as build, \
                    patch.object(tool, "stage") as stage, patch("sys.stderr"):
                with self.assertRaises(SystemExit) as exit:
                    tool.main()
                self.assertEqual(exit.exception.code, 2)
                build.assert_not_called()
                stage.assert_not_called()

    def test_stage_requires_confirmation_before_build_or_write(self):
        tool = module(ROOT / "tools/ccs.py", "ccs_tool")
        with patch.object(tool, "build") as build:
            with self.assertRaisesRegex(RuntimeError, "confirmation"):
                tool.stage()
            build.assert_not_called()
            with self.assertRaisesRegex(RuntimeError, "confirmation"):
                tool.stage(attack=True)
            build.assert_not_called()

    def test_stage_refuses_live_game(self):
        tool = module(ROOT / "tools/ccs.py", "ccs_tool")
        with patch.object(tool, "game_processes", return_value=[123]), patch.object(tool, "build") as build:
            with self.assertRaisesRegex(RuntimeError, "running"):
                tool.stage(True)
            build.assert_not_called()
            with self.assertRaisesRegex(RuntimeError, "running"):
                tool.stage(True, attack=True)
            build.assert_not_called()

    def test_copy_verifies_before_replacing(self):
        tool = module(ROOT / "tools/ccs.py", "ccs_tool")
        with tempfile.TemporaryDirectory(dir=ROOT / "work/takeover") as temporary:
            root = Path(temporary)
            source, target = root / "source", root / "target"
            source.write_text("new")
            target.write_text("old")
            with patch.object(tool, "sha", side_effect=["a", "b"]):
                with self.assertRaisesRegex(RuntimeError, "verification"):
                    tool.copy_verified(source, target)
            self.assertEqual(target.read_text(), "old")
            tool.copy_verified(source, target)
            self.assertEqual(target.read_text(), "new")

    def test_catalog_matches_sources(self):
        generator = module(ROOT / "work/takeover/generate_catalog.py", "ccs_catalog_generator")
        self.assertEqual(generator.generate(), json.loads((ROOT / "data/catalog.json").read_text()))


if __name__ == "__main__":
    unittest.main()

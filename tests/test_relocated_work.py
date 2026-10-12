"""Exercise tool path guards without importing tools that control the game."""
import ast
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]


class RelocatedWorkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.repo = self.base / 'workspace/CustomShellSystem'
        self.repo.mkdir(parents=True)
        self.physical = self.base / 'external/CSS-work'
        self.physical.mkdir(parents=True)
        (self.repo / 'work').symlink_to(self.physical, target_is_directory=True)
        self.outside = self.base / 'outside'
        self.outside.mkdir()

    def test_authoring_containment_guards(self):
        count = 0
        for path in (ROOT / 'tools').rglob('*.py'):
            for node in ast.walk(ast.parse(path.read_text())):
                if not (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                        and node.func.attr == 'is_relative_to' and len(node.args) == 1):
                    continue
                argument = node.args[0]
                constants = [n.value for n in ast.walk(argument)
                             if isinstance(n, ast.Constant) and isinstance(n.value, str)]
                parts = [s for s in constants if s == 'work' or s.startswith('work/')
                         or s == 'CustomShellSystem/work' or s.startswith('CustomShellSystem/work/')]
                if len(parts) != 1:
                    continue
                part = parts[0]
                root = self.repo.parent if part.startswith('CustomShellSystem/') else self.repo
                env = {name: root for name in ('ROOT', 'root', 'CSS', 'REPO')}
                logical = root / part
                logical.mkdir(parents=True, exist_ok=True)
                escape = logical / 'escape'
                if not escape.exists():
                    escape.symlink_to(self.outside, target_is_directory=True)
                # Substitute only the candidate path; retain the tool's actual guard.
                node.func.value = ast.parse('candidate.resolve()', mode='eval').body
                expression = compile(ast.fix_missing_locations(ast.Expression(node)), str(path), 'eval')
                cases = [(logical / 'trial', True), (logical.resolve() / 'trial', True),
                         (logical / 'escape/trial', False), (self.outside / 'trial', False),
                         (Path(str(logical.resolve()) + '-other') / 'trial', False)]
                for candidate, expected in cases:
                    with self.subTest(tool=str(path.relative_to(ROOT)), candidate=str(candidate)):
                        self.assertEqual(eval(expression, {'__builtins__': {}}, env | {'candidate': candidate}), expected)
                count += 1
        self.assertGreater(count, 80)

    def test_cook_accepts_relocated_work_but_not_adjacent_storage(self):
        path = ROOT / 'tools/shared-assets/cook-namespace-trial.py'
        spec = importlib.util.spec_from_file_location('namespace_cook', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with patch.object(module, '__file__', str(self.repo / 'tools/shared-assets/cook-namespace-trial.py')):
            self.assertTrue(module.private_storage(self.repo / 'work/cook'))
            self.assertTrue(module.private_storage(self.physical / 'cook'))
            self.assertFalse(module.private_storage(self.physical))
            self.assertFalse(module.private_storage(self.outside / 'cook'))
            (self.physical / 'escape').symlink_to(self.outside, target_is_directory=True)
            self.assertFalse(module.private_storage(self.repo / 'work/escape/cook'))
            (self.repo / 'work').unlink()
            self.assertTrue(module.private_storage(Path('/home/css-cook-check/new')))

    def test_grip_studies_require_the_exact_relocated_parent(self):
        count = 0
        for path in (ROOT / 'tools/authoring-probes').rglob('*.py'):
            for node in ast.walk(ast.parse(path.read_text())):
                if not isinstance(node, ast.Compare) or len(node.ops) != 1 or not isinstance(node.ops[0], ast.NotEq):
                    continue
                if not any(isinstance(n, ast.Constant) and n.value == 'work/grip-grounding-v1' for n in ast.walk(node)):
                    continue
                env = {'Path': Path, '__file__': str(self.repo / path.relative_to(ROOT))}
                expression = compile(ast.Expression(node), str(path), 'eval')
                for out, expected in [(self.repo / 'work/grip-grounding-v1/trial', False),
                                      (self.physical / 'grip-grounding-v1/trial', False),
                                      (self.outside / 'trial', True)]:
                    with self.subTest(tool=str(path.relative_to(ROOT)), output=str(out)):
                        self.assertEqual(eval(expression, {'__builtins__': {}}, env | {'OUT': out}), expected)
                count += 1
        self.assertEqual(count, 11)

    def test_probe_repository_paths_do_not_follow_output_storage(self):
        count = 0
        for directory in ('arm-rest', 'arm-twist', 'hand-transfer', 'game-foundation'):
            for path in (ROOT / 'tools/authoring-probes' / directory).glob('*.py'):
                for node in ast.walk(ast.parse(path.read_text())):
                    if not isinstance(node, ast.Assign) or not any(
                            isinstance(t, ast.Name) and t.id in ('root', 'ROOT') for t in node.targets):
                        continue
                    expression = ast.unparse(node.value)
                    if expression != 'Path(__file__).resolve().parents[4]':
                        self.assertNotEqual(expression, 'OUT.parents[3]', str(path))
                        continue
                    env = {'Path': Path, '__file__': str(self.repo / path.relative_to(ROOT)),
                           'OUT': self.physical / 'grip-grounding-v1/trial'}
                    with self.subTest(tool=str(path.relative_to(ROOT))):
                        self.assertEqual(eval(compile(ast.Expression(node.value), str(path), 'eval'),
                                              {'__builtins__': {}}, env), self.repo.parent)
                    count += 1
        self.assertGreaterEqual(count, 13)

    def test_exact_path_guards_accept_both_spellings(self):
        count = 0
        for path in (ROOT / 'tools').rglob('*.py'):
            for node in ast.walk(ast.parse(path.read_text())):
                if not isinstance(node, ast.Compare) or len(node.ops) != 1 or not isinstance(node.ops[0], (ast.Eq, ast.NotEq)):
                    continue
                rhs = node.comparators[0]
                if not (isinstance(rhs, ast.BinOp) and isinstance(rhs.op, ast.Div) or
                        isinstance(rhs, ast.Call) and isinstance(rhs.func, ast.Attribute) and rhs.func.attr == 'resolve'):
                    continue
                values = [n.value for n in ast.walk(rhs) if isinstance(n, ast.Constant) and isinstance(n.value, str)]
                parts = [s for s in values if s == 'work' or s.startswith('work/') or s.startswith('CustomShellSystem/work/')]
                if len(parts) != 1:
                    continue
                root = self.repo.parent if parts[0].startswith('CustomShellSystem/') else self.repo
                env = {name: root for name in ('ROOT', 'CSS', 'MOD', 'mod')}
                env.update(Path=Path, __file__=str(self.repo / path.relative_to(ROOT)), REVISION='V1')
                target = eval(compile(ast.Expression(rhs), str(path), 'eval'), {'__builtins__': {}}, env)
                logical = self.repo / 'work' / target.relative_to(self.physical)
                uses_parent = any(isinstance(n, ast.Attribute) and n.attr == 'parent' for n in ast.walk(node.left))
                expression = compile(ast.Expression(node), str(path), 'eval')
                for candidate, same in [(logical, True), (target, True), (self.outside, False)]:
                    value = candidate / 'trial' if uses_parent else candidate
                    variables = {name: value for name in ('OUT', 'WORK', 'TARGET', 'candidate')}
                    expected = same if isinstance(node.ops[0], ast.Eq) else not same
                    with self.subTest(tool=str(path.relative_to(ROOT)), candidate=str(candidate)):
                        self.assertEqual(eval(expression, {'__builtins__': {}}, env | variables), expected)
                count += 1
        self.assertEqual(count, 20)


if __name__ == '__main__':
    unittest.main()

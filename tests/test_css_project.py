import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import css_project as project
import css_package as package
import css_color_package as color_package


def thumbnail(path):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 128, 128, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress((b'\0' + bytes([180, 120, 80, 255]) * 128) * 128))
                     + chunk(b'IEND', b''))


class ProjectTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.file = project.initialize(self.root / 'outfit', 'author.outfit', 'My Outfit', 'Author')
        thumbnail(self.file.parent / 'thumbnail.png')
        (self.file.parent / 'source' / 'Original_P.pak').write_bytes(b'source container fixture')
        self.data = json.loads(self.file.read_text())

    def save(self, data):
        self.file.write_text(json.dumps(data))

    def test_paths_are_relative_to_project_not_current_directory(self):
        args = project.read_project(self.file)
        self.assertEqual(args.inputs, [self.file.parent / 'source'])
        self.assertEqual(args.thumbnail, self.file.parent / 'thumbnail.png')
        self.assertEqual(args.id, 'author.outfit')
        self.assertFalse(hasattr(args, 'game'))
        with self.assertRaises(FileExistsError):
            project.initialize(self.file.parent, 'other.outfit', 'Other', 'Other')

    def test_missing_input_and_unknown_fields_do_not_pass(self):
        cases = [dict(self.data, colour='typo.json'), dict(self.data, id='../outfit'),
                 dict(self.data, inputs=[]), dict(self.data, inputs='source'),
                 dict(self.data, thumbnail='missing.png'), dict(self.data, format_version=2)]
        for data in cases:
            self.save(data)
            with self.subTest(data=data), self.assertRaises((ValueError, FileNotFoundError)):
                project.read_project(self.file)

    def test_grouped_recipe_and_color_identity(self):
        colors = {'id': 'author.outfit', 'colors': {'schema': 1, 'controls': [
            {'id': 'cloth', 'name': 'Cloth', 'default': [1, 1, 1, 1],
             'bindings': [{'slot': 0, 'parameter': 'ClothTint'}]}], 'palettes': []}}
        color_file = self.file.parent / 'colors.json'
        color_file.write_text(json.dumps(colors))
        groups = [{'id': 'cape', 'name': 'Cape', 'inputs': ['source'], 'colors': 'colors.json'}]
        (self.file.parent / 'variants.json').write_text(json.dumps(groups))
        data = copy.deepcopy(self.data)
        del data['inputs']
        data['variant_sources'] = 'variants.json'
        self.save(data)
        self.assertIsNotNone(project.read_project(self.file).variant_sources)
        colors['id'] = 'wrong.outfit'
        color_file.write_text(json.dumps(colors))
        with self.assertRaisesRegex(ValueError, 'Control recipe ID'):
            project.read_project(self.file)
        self.save(dict(data, inputs=['source']))
        with self.assertRaisesRegex(ValueError, 'owns inputs'):
            project.read_project(self.file)

    def test_build_forwards_paths_and_version_then_verifies(self):
        game = self.root / 'game'; game.mkdir()
        tool = self.root / 'tool'; tool.touch()
        output = self.root / 'output'
        converted = output / 'CSS_My_Outfit_Author_P'
        with patch.object(project.converter, 'convert', return_value=converted) as convert, \
             patch.object(project, 'verify') as verify, patch.object(project, 'release_zip') as zip_it:
            package_path, archive = project.build(self.file, game, tool, tool, output)
            args = convert.call_args.args[0]
            self.assertEqual((args.game, args.retoc, args.repak), (game, tool, tool))
            self.assertEqual(args.package_version, '1.0.0')
            self.assertEqual(package_path, converted)
            self.assertEqual(archive, output / 'CSS_My_Outfit_Author_P.zip')
            verify.assert_called_once_with(converted, tool)
            zip_it.assert_called_once_with(converted, archive, tool)


class ReleaseZipTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'source'; self.source.mkdir()
        for ext in ('pak', 'utoc', 'ucas'):
            (self.source / f'CSS_Example_Author_P.{ext}').write_bytes((ext * 64).encode())
        self.output = self.root / 'release.zip'

    def test_one_install_folder_and_original_bytes(self):
        # Container validation is exercised by the real local conversion separately.
        with patch.object(package, 'verify') as verify:
            package.release_zip(self.source, self.output)
            self.assertEqual(verify.call_count, 3)
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(set(archive.namelist()), {f'CSS_Example_Author_P/{p.name}' for p in self.source.iterdir()})
            for path in self.source.iterdir():
                self.assertEqual(archive.read(f'CSS_Example_Author_P/{path.name}'), path.read_bytes())
        before = self.output.read_bytes()
        with self.assertRaises(FileExistsError):
            package.release_zip(self.source, self.output)
        self.assertEqual(before, self.output.read_bytes())

    def test_snapshot_failure_never_publishes(self):
        with patch.object(package, 'verify', side_effect=[{}, ValueError('changed input')]):
            with self.assertRaisesRegex(ValueError, 'changed input'):
                package.release_zip(self.source, self.output)
        self.assertFalse(self.output.exists())


    def test_interrupted_final_write_removes_partial_zip(self):
        original = package.shutil.copyfileobj
        def fail(source, target, *args, **kwargs):
            if Path(str(getattr(target, 'name', ''))) == self.output:
                target.write(b'partial')
                raise OSError('interrupted')
            return original(source, target, *args, **kwargs)
        with patch.object(package, 'verify'), patch.object(package.shutil, 'copyfileobj', side_effect=fail):
            with self.assertRaisesRegex(OSError, 'interrupted'):
                package.release_zip(self.source, self.output)
        self.assertFalse(self.output.exists())


class ColorUpdateTests(unittest.TestCase):
    def test_variant_recipes_are_not_silently_replaced(self):
        manifest = {'catalog': {'outfits': [{'variants': [{'id': 'cape', 'colors': {'schema': 1}}]}]}}
        with patch.object(color_package, 'verify', return_value=manifest):
            with self.assertRaisesRegex(ValueError, 'variant-specific'):
                color_package.build(Path('unused'), Path('unused'), Path('unused'))

class ColorTemplateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def test_generates_valid_bindings_template(self):
        import css_colors
        target = self.root / "colors.json"
        project.generate_color_template(target, "author.outfit", "bindings")
        self.assertTrue(target.exists())
        data = json.loads(target.read_text())
        self.assertEqual(data["id"], "author.outfit")
        css_colors.validate(data["colors"])
        problems = css_colors.lint_convention(data["colors"])
        self.assertEqual(problems, [])

    def test_generates_valid_dye_template(self):
        import css_colors
        target = self.root / "colors_dye.json"
        project.generate_color_template(target, "author.outfit", "dye")
        self.assertTrue(target.exists())
        data = json.loads(target.read_text())
        self.assertIn("surfaces", data["colors"])
        css_colors.validate(data["colors"])


if __name__ == '__main__':
    unittest.main()

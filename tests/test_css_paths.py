import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import css_paths as paths


class PathsTests(unittest.TestCase):
    def test_asset_namespace_and_old_binary_padding(self):
        paths.package_path('/Game/CSS/SeduXtress/SK_Body')
        for bad in ('/Game/CSSAuthoring/SK_Body', '/Game/CSS/',
                    '/Game/CSS/' + 'a11b15f3a68492900791118919d432f7' + '/MI_Body',
                    '/Game/CSS/A/../SK_Body', '/Game/CSS/CON/SK_Body',
                    '/Game/CSS/A/SK_Body.SK_Body'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                paths.package_path(bad)

    def test_full_windows_destination_not_only_basename(self):
        relative = 'MortalShell2/Content/CSS/SeduXtress/SK_Body.uasset'
        paths.windows_destination('C:/Mods', relative)
        with self.assertRaises(ValueError):
            paths.windows_destination('C:/' + 'folder/' * 30, relative)
        for relative in ('../outside', 'D:/outside', '/outside', 'A/NUL.txt'):
            with self.subTest(relative=relative), self.assertRaises(ValueError):
                paths.windows_destination('C:/Mods', relative)

    def test_sequential_scratch_preserves_existing_directory(self):
        root = Path(__file__).resolve().parents[1] / 'work/tmp'
        with paths.temporary_directory(root, 'paths') as test:
            first = paths.new_directory(test, 'build')
            marker = first / 'keep.txt'
            marker.write_text('keep')
            second = paths.new_directory(test, 'build')
            self.assertEqual((first.name, second.name), ('build-0001', 'build-0002'))
            self.assertEqual(marker.read_text(), 'keep')


if __name__ == '__main__':
    unittest.main()

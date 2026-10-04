import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

path = Path(__file__).resolve().parents[1] / 'tools/shared-assets/release.py'
spec = importlib.util.spec_from_file_location('astral_release', path)
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class AstralReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.archive = Path(self.temp.name) / 'shared.zip'
        self.files = {name: b'fixture' for name in release.ASSETS}
        self.files['README.txt'] = b'CSS v1.0.0-beta.11'
        self.files['release.json'] = json.dumps({
            'version': '1.0.0-beta.11', 'css_version': '1.0.0-beta.11',
            'container': release.CONTAINER,
            'files': {name: release.digest(data) for name, data in self.files.items()},
        }).encode()

    def write(self):
        with zipfile.ZipFile(self.archive, 'w') as z:
            for name, data in self.files.items():
                z.writestr(release.PREFIX + name, data)

    def test_complete_archive(self):
        self.write()
        self.assertEqual(release.verify(self.archive)['css_version'], '1.0.0-beta.11')

    def test_modified_container_rejected(self):
        self.files[release.CONTAINER + '.ucas'] = b'changed'
        self.write()
        with self.assertRaisesRegex(ValueError, 'checksum'):
            release.verify(self.archive)

    def test_unrelated_or_personal_files_rejected(self):
        for name in ('state.json', '../CSS_SharedAssets_P.ucas', 'other.pak'):
            with self.subTest(name=name):
                self.files[name] = b'extra'
                self.write()
                with self.assertRaisesRegex(ValueError, 'Unexpected'):
                    release.verify(self.archive)
                del self.files[name]

    def test_missing_container_rejected(self):
        del self.files[release.CONTAINER + '.utoc']
        self.write()
        with self.assertRaisesRegex(ValueError, 'Unexpected'):
            release.verify(self.archive)


if __name__ == '__main__':
    unittest.main()

"""Exercise the runtime ZIP allowlist and payload integrity checks."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from css_release import PREFIX, digest, payload_names, verify


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.archive = Path(self.temp.name) / 'release.zip'
        dll = bytearray(128)
        dll[:2] = b'MZ'
        struct.pack_into('<I', dll, 60, 64)
        dll[64:68] = b'PE\0\0'
        struct.pack_into('<H', dll, 68, 0x8664)
        struct.pack_into('<H', dll, 86, 0x2000)
        self.files = {p: b'' for p in payload_names('0.1.1')}
        self.files.update({'dlls/main.dll': bytes(dll), 'cores/css_core-0.1.1.dll': bytes(dll),
                           'README.txt': b'MSII - CSS v0.1.1',
                           'core.json': json.dumps({'abi': 1, 'file': 'css_core-0.1.1.dll'}).encode()})
        self.files['release.json'] = json.dumps({'version': '0.1.1', 'interface': 'inventory',
            'files': {p: digest(data) for p, data in self.files.items()}}).encode()

    def write(self):
        with zipfile.ZipFile(self.archive, 'w') as z:
            for p, data in self.files.items():
                z.writestr(PREFIX + p, data)

    def test_minimal_archive(self):
        self.write()
        self.assertEqual(verify(self.archive)['version'], '0.1.1')

    def test_alpha_archive_uses_matching_versioned_core(self):
        version = '1.0.0-alpha.1'
        self.files['cores/css_core-' + version + '.dll'] = self.files.pop('cores/css_core-0.1.1.dll')
        self.files['README.txt'] = ('MSII - CSS v' + version).encode()
        self.files['core.json'] = json.dumps({'abi': 1, 'file': 'css_core-' + version + '.dll'}).encode()
        del self.files['release.json']
        self.files['release.json'] = json.dumps({'version': version, 'interface': 'inventory',
            'files': {p: digest(data) for p, data in self.files.items()}}).encode()
        self.write()
        self.assertEqual(verify(self.archive)['version'], version)

    def test_personal_state_and_old_cores_cannot_ship(self):
        for path in ['state/state.json', 'request.json', 'CSS.log', 'cores/css_core-old.dll', 'cache/thumbnail.png']:
            with self.subTest(path=path):
                self.files[path] = b'private data'
                self.write()
                with self.assertRaisesRegex(ValueError, 'unexpected or missing'):
                    verify(self.archive)
                del self.files[path]

    def test_missing_readme_rejected(self):
        del self.files['README.txt']
        self.write()
        with self.assertRaisesRegex(ValueError, 'unexpected or missing'):
            verify(self.archive)

    def test_old_standalone_archive_still_verifies(self):
        del self.files['assets/inventory-logo-v1.png']
        self.files['assets/wardrobe-v1.png'] = b'old artwork'
        manifest = json.loads(self.files['release.json'])
        del manifest['files']['assets/inventory-logo-v1.png']
        del manifest['interface']
        manifest['files']['assets/wardrobe-v1.png'] = digest(self.files['assets/wardrobe-v1.png'])
        self.files['release.json'] = json.dumps(manifest).encode()
        self.write()
        self.assertEqual(verify(self.archive)['version'], '0.1.1')

    def test_inventory_archive_does_not_ship_old_artwork(self):
        self.assertNotIn('assets/wardrobe-v1.png', payload_names('0.1.1'))
        self.files['assets/wardrobe-v1.png'] = b'unused'
        self.write()
        with self.assertRaisesRegex(ValueError, 'unexpected or missing'):
            verify(self.archive)

    def test_modified_payload_rejected(self):
        self.files['dlls/main.dll'] += b'changed'
        self.write()
        with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
            verify(self.archive)

    def test_core_selection_must_match_bundled_dll(self):
        self.files['core.json'] = b'{"abi": 1, "file": "css_core-old.dll"}'
        manifest = json.loads(self.files['release.json'])
        manifest['files']['core.json'] = digest(self.files['core.json'])
        self.files['release.json'] = json.dumps(manifest).encode()
        self.write()
        with self.assertRaisesRegex(ValueError, 'Core selection'):
            verify(self.archive)


if __name__ == '__main__':
    unittest.main()

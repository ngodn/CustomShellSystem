"""Verify the separate framework ZIP cannot include development or user data."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from css_release import digest
from cssx_release import framework_names, verify_framework


class CSSXReleaseTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.archive = Path(temporary.name)/'CSSX.zip'
        dll = bytearray(128)
        dll[:2] = b'MZ'
        struct.pack_into('<I', dll, 60, 64)
        dll[64:68] = b'PE\0\0'
        struct.pack_into('<H', dll, 68, 0x8664)
        struct.pack_into('<H', dll, 86, 0x2000)
        self.files = {p: b'' for p in framework_names('0.3.0')}
        self.files['cores/cssx_core-0.3.0.dll'] = bytes(dll)
        self.files['cssx.json'] = b'{"file":"cssx_core-0.3.0.dll"}'

    def write(self):
        files = dict(self.files)
        files['cssx-release.json'] = json.dumps({'version':'0.3.0', 'files':{p:digest(v) for p,v in self.files.items()}}).encode()
        with zipfile.ZipFile(self.archive, 'w') as archive:
            for p, data in files.items(): archive.writestr(p, data)

    def test_minimal_framework(self):
        self.write()
        self.assertEqual(verify_framework(self.archive)['version'], '0.3.0')

    def test_private_files_rejected_even_with_valid_checksum(self):
        for path in ['state/extensions/test.json','logs/cssx.jsonl','cores/cssx_core-dev.dll','extensions/cheat-menu/extension.json','core.json']:
            with self.subTest(path=path):
                self.files[path] = b'private'
                self.write()
                with self.assertRaisesRegex(ValueError,'unexpected or missing'): verify_framework(self.archive)
                del self.files[path]

    def test_stale_core_selection(self):
        self.files['cssx.json'] = b'{"file":"cssx_core-old.dll"}'
        self.write()
        with self.assertRaisesRegex(ValueError,'selection mismatch'): verify_framework(self.archive)

    def test_wrong_architecture(self):
        value=bytearray(self.files['cores/cssx_core-0.3.0.dll'])
        struct.pack_into('<H',value,68,0x14c)
        self.files['cores/cssx_core-0.3.0.dll']=bytes(value)
        self.write()
        with self.assertRaisesRegex(ValueError,'x64 Windows DLL'): verify_framework(self.archive)


if __name__ == '__main__': unittest.main()

import importlib.util
from pathlib import Path
import struct
import unittest

source = Path(__file__).resolve().parents[1] / 'tools/convert_beaute.py'
spec = importlib.util.spec_from_file_location('conversion', source)
conversion = importlib.util.module_from_spec(spec)
spec.loader.exec_module(conversion)


class ConversionTests(unittest.TestCase):
    def test_live_headers_roundtrip(self):
        assets = list((source.parents[1] / 'work/conversion-v3/legacy').rglob('*.uasset'))
        self.assertEqual(len(assets), 10)
        for path in assets:
            original = path.read_bytes()
            entries = conversion.names(original)
            mapping = {n: n.replace('/Game/Sparta/', '/Game/CSSB01/', 1)
                       for _, _, n in entries if n.startswith('/Game/Sparta/')}
            patched, changes = conversion.rename_header(original, mapping)
            self.assertTrue(changes)
            restored, _ = conversion.rename_header(patched, {v: k for k, v in mapping.items()})
            self.assertEqual(original, restored)

    def test_invalid_headers(self):
        for data in (b'', b'1234', bytes(1024)):
            with self.assertRaises((ValueError, struct.error)):
                conversion.names(data)

    def test_resize_is_rejected(self):
        path = next((source.parents[1] / 'work/conversion-v3/legacy').rglob('*.uasset'))
        data = path.read_bytes()
        name = conversion.names(data)[0][2]
        with self.assertRaises(ValueError):
            conversion.rename_header(data, {name: name + '_new'})

    def test_no_unresolved_imports(self):
        for path in (source.parents[1] / 'work/conversion-v3/check').rglob('*.uasset'):
            for _, _, name in conversion.names(path.read_bytes()):
                self.assertNotIn('UnknownPackage', name)
                self.assertNotIn('UnknownExport', name)


if __name__ == '__main__':
    unittest.main()

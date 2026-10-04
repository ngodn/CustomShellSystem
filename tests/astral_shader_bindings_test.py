"""Malformed UE resource headers must fail before producing texture identities."""
import importlib.util
from pathlib import Path
import struct
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "tools/diagnostics/genessa-doubles/read-material-bindings.py"
SPEC = importlib.util.spec_from_file_location("astral_bindings", SCRIPT)
bindings = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bindings)


def header(texture=None, bits=2):
    # Two buffer slots. Buffer 1 owns resource 0, bound at texture register 5.
    arrays = [[], [], [], [0, 0x12345678],
              [0, 2, 0x01000005, 0xffffffff] if texture is None else texture, []]
    result = struct.pack("<I", bits)
    for values in arrays:
        result += struct.pack("<i", len(values))
        result += struct.pack(f"<{len(values)}I", *values)
    return result


class ResourceHeaderTests(unittest.TestCase):
    def parse(self, value):
        return bindings.resource_table(value + b"DXBC", len(value))

    def test_known_binding_and_round_trip(self):
        hashes, table = self.parse(header())
        self.assertEqual(hashes, [0, 0x12345678])
        self.assertEqual(table["texture"], [{"buffer": 1, "resource": 0, "bind": 5}])

    def test_single_field_change_preserves_other_fields(self):
        before = header()
        after = header(texture=[0, 2, 0x01000006, 0xffffffff])
        self.assertEqual(sum(a != b for a, b in zip(before, after)), 1)
        self.assertEqual(self.parse(after)[1]["texture"], [{"buffer": 1, "resource": 0, "bind": 6}])

    def test_truncated_header(self):
        for length in range(len(header())):
            with self.subTest(length=length), self.assertRaises(ValueError):
                self.parse(header()[:length])

    def test_negative_array_count(self):
        value = bytearray(header())
        struct.pack_into("<i", value, 4, -1)
        with self.assertRaises(ValueError):
            self.parse(value)

    def test_malformed_token_streams(self):
        for stream in ([0, 3, 0x01000005, 0xffffffff],
                       [0, 2, 0x01000005, 0],
                       [0, 2, 0x01000005, 0x01000005, 0xffffffff],
                       [0, 2, 0x01000006, 0x01000005, 0xffffffff],
                       [0, 2, 0x02000005, 0xffffffff]):
            with self.subTest(stream=stream), self.assertRaises(ValueError):
                self.parse(header(texture=stream))

    def test_inactive_or_unbounded_buffer(self):
        for bits in (0, 4):
            with self.subTest(bits=bits), self.assertRaises(ValueError):
                self.parse(header(bits=bits))

    def test_wrong_container_boundary(self):
        value = header()
        with self.assertRaises(ValueError):
            bindings.resource_table(value + b"JUNKDXBC", len(value))
        with self.assertRaises(ValueError):
            bindings.resource_table(value + b"JUNKDXBC", len(value) + 4)


if __name__ == "__main__":
    unittest.main()

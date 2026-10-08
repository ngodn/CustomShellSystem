"""Verify streaming digests against the exact UTF-8 bytes of full UE snapshots.

Run with: uv run --no-project --with blake3 python <script> <probe-directory>
"""
import argparse
import hashlib
import json
from pathlib import Path

from blake3 import blake3


def compare(raw, expected):
    decoder = json.JSONDecoder()
    arrays = 0
    rows = 0

    def walk(offset, target):
        nonlocal arrays, rows
        if isinstance(target, dict) and set(target) == {'count', 'utf8_bytes', 'blake3'}:
            value, end = decoder.raw_decode(raw, offset)
            assert isinstance(value, list) and len(value) == target['count']
            data = raw[offset:end].encode('utf-8')
            assert len(data) == target['utf8_bytes']
            assert blake3(data).hexdigest() == target['blake3'].lower()
            arrays += 1
            rows += len(value)
            return end
        if isinstance(target, dict):
            assert raw[offset] == '{'
            offset += 1
            seen = set()
            while raw[offset] != '}':
                if seen:
                    assert raw[offset] == ','
                    offset += 1
                key, offset = decoder.raw_decode(raw, offset)
                assert key in target and key not in seen and raw[offset] == ':'
                seen.add(key)
                offset = walk(offset + 1, target[key])
            assert seen == set(target)
            return offset + 1
        if isinstance(target, list):
            assert raw[offset] == '['
            offset += 1
            for index, value in enumerate(target):
                if index:
                    assert raw[offset] == ','
                    offset += 1
                offset = walk(offset, value)
            assert raw[offset] == ']'
            return offset + 1
        value, end = decoder.raw_decode(raw, offset)
        assert value == target
        return end

    assert walk(0, expected) == len(raw)
    return dict(arrays=arrays, rows=rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    report = {}
    for kind in ('render', 'source'):
        full = (args.probe / (kind + '-full.json')).read_bytes()
        summary = (args.probe / (kind + '-digest.json')).read_bytes()
        report[kind] = dict(compare(full.decode('utf-8'), json.loads(summary)),
                            full_sha256=hashlib.sha256(full).hexdigest(),
                            summary_sha256=hashlib.sha256(summary).hexdigest())
    report['passed'] = True
    (args.probe / 'independent-comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()

"""Independently audit the eight completed receipts and their recovered payloads."""
import json
from pathlib import Path
import struct
import subprocess
import sys

from build_pack import HERE, PACKS, WORK, digest, prepare, record, require, verify_exports, write_json


def main():
    results = []
    for pack in PACKS:
        receipt_path = WORK / pack / "receipt.json"
        receipt = json.loads(receipt_path.read_text())
        require(receipt["status"] == "passed", "Failed receipt: " + pack)
        run = Path(receipt["run"])
        spec, rows = prepare(pack)
        require(spec == json.loads((run / "stage/input.json").read_text()), "Inputs changed since build")
        require(rows == json.loads((run / "stage/rows.json").read_text()), "Rows changed since build")
        for item in receipt["trio"] + receipt["inputs"] + receipt["cooked_files"] + [receipt["canonical_rows"]]:
            require(record(Path(item["path"])) == item, "Receipt hash differs: " + item["path"])
        require(all(Path(path).is_file() for path in receipt["logs"]), "Receipt points to a missing log")
        summaries = {}
        for log in (run / "stage/author.log", run / "stage/reload.log", run / "logs/cook.log"):
            require("Success - 0 error(s), 0 warning(s)" in log.read_text(errors="replace"), "Commandlet summary differs: " + str(log))
            summaries[str(log)] = "0 errors, 0 warnings"
        audio = verify_exports(run / "dump", spec, rows)
        payloads = []
        for cooked in sorted((run / "legacy").rglob("*")):
            if cooked.suffix not in (".uexp", ".ubulk"):
                continue
            relative = cooked.relative_to(run / "legacy")
            recovered = run / "roundtrip" / relative
            require(recovered.is_file() and digest(cooked) == digest(recovered), "Payload roundtrip differs: " + str(relative))
            payloads.append({"relative": str(relative), "bytes": cooked.stat().st_size, "sha256": digest(cooked)})
        headers = {}
        for audio_id, expected in spec["audio"].items():
            name = expected["asset"].rsplit(".", 1)[1]
            path = run / "roundtrip/MortalShell2/Content/CSSX/CINE" / pack / "Audio" / (name + ".uexp")
            body = path.read_bytes()
            bulk = next(iter(audio[audio_id]["formats"].values()))
            require("ForceInlinePayload" in bulk["BulkDataFlags"], "Expected inline audio")
            offset = int(bulk["OffsetInFile"], 16)
            header = struct.unpack_from("<4sBBHIIHHIHH", body, offset)
            tag, version, channels, _, rate, frames, _, _, size, seek_entries, seek_blocks = header
            # binka_ue_encode.cpp writes the multichar constant 'UEBA', little-endian ABEU.
            require(tag == b"ABEU" and version == 1, "Unexpected Bink header")
            require((channels, rate, frames) == (expected["channels"], expected["rate"], expected["frames"]), "Encoded audio differs from WAV")
            require(size == bulk["SizeOnDisk"] and offset + size <= len(body), "Truncated Bink payload")
            headers[audio_id] = {"channels": channels, "rate": rate, "frames": frames, "compressed_bytes": size,
                                 "seek_entries": seek_entries, "blocks_per_seek_entry": seek_blocks,
                                 "recovered_export_sha256": digest(path)}
        subprocess.run([sys.executable, "-B", str(HERE / "check_readback.py"), str(run)], check=True)
        results.append({"pack": pack, "receipt": record(receipt_path), "rows": len(rows),
                        "source_bytes": receipt["source_bytes"], "trio_bytes": receipt["trio_bytes"],
                        "payloads": payloads, "encoded_audio": headers,
                        "commandlet_summaries": summaries,
                        "readback_controls": str(run / "readback-controls.json")})
    require(sum(len(PACKS[p][2]) for p in PACKS) == 32, "Preset membership differs")
    require(sum(len(r["encoded_audio"]) for r in results) == 16, "Audio membership differs")
    write_json(WORK / "validation.json", {"status": "passed", "packs": results,
               "total_source_bytes": sum(r["source_bytes"] for r in results),
               "total_trio_bytes": sum(r["trio_bytes"] for r in results),
               "total_rows": sum(r["rows"] for r in results),
               "total_audio": sum(len(r["encoded_audio"]) for r in results)})
    print("PASS: eight packs, 32 presets, 16 encoded tracks, all receipt hashes and recovered payloads")


if __name__ == "__main__":
    main()

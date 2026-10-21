"""Copy the Genessa seal transform into current-game raw IoStore chunks.

Inputs come from AssetReadback --socket-slices (Debug build) and retoc
unpack-raw. Packed output still requires independent property readback.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def rebase(current, previous, raw, output):
    socket = "Socket_Prop_Stowed_InfiniteSeal_Right"
    new = {r["package"]: r for r in json.loads(current.read_text())}
    old = {r["package"]: r for r in json.loads(previous.read_text())}
    if not new or new.keys() != old.keys():
        raise ValueError("Socket package sets differ")
    manifest = json.loads((raw / "manifest.json").read_text())
    chunks = {"/Game/" + p.split("/Content/")[1].removesuffix(".uasset"): chunk
              for chunk, p in manifest["chunk_paths"].items()}
    if chunks.keys() != new.keys():
        raise ValueError("Raw package set differs from socket readback")
    replacements, records = {}, []
    for package, row in new.items():
        a = {s["name"]: s for s in row["slices"]}
        b = {s["name"]: s for s in old[package]["slices"]}
        if a.keys() != b.keys() or socket not in a:
            raise ValueError("Socket layout changed: " + package)
        if any(a[k]["hex"] != b[k]["hex"] for k in a if k != socket):
            raise ValueError("Other sockets changed; manual review required: " + package)
        data = (raw / "chunks" / chunks[package]).read_bytes()
        if hashlib.sha256(data).hexdigest() != row["sha256"]:
            raise ValueError("Raw input differs from decoded package: " + package)
        start = a[socket]["offset"]
        before, after = bytes.fromhex(a[socket]["hex"]), bytes.fromhex(b[socket]["hex"])
        if len(before) != len(after) or data[start:start + len(before)] != before:
            raise ValueError("Transform span mismatch: " + package)
        patched = data[:start] + after + data[start + len(before):]
        replacements[chunks[package]] = patched
        records.append(dict(package=package, chunk=chunks[package], offset=start,
                            bytes=len(after), before_sha256=row["sha256"],
                            after_sha256=hashlib.sha256(patched).hexdigest()))
    shutil.copytree(raw, output)
    for chunk, data in replacements.items():
        (output / "chunks" / chunk).write_bytes(data)
    return records


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("current", "previous", "raw", "output", "report"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    if args.output.exists() or args.report.exists():
        parser.error("Output and report must be new paths")
    records = rebase(args.current, args.previous, args.raw, args.output)
    args.report.write_text(json.dumps(records, indent=2) + "\n")
    print(f"Preserved seal transforms in {len(records)} current-game packages")

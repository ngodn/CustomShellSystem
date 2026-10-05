"""Inventory the supplied CSS/CINE animation archives without extracting or running them."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "extensions/cine/docs/reference/animation"
OUTPUT = ROOT / "work/cine-animation-port/inventory.json"


def safe_name(name):
    path = PurePosixPath(name.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts or any(":" in p for p in path.parts):
        raise ValueError(f"Unsafe archive member: {name!r}")
    return str(path)


def zip_members(path):
    rows = []
    with zipfile.ZipFile(path) as archive:
        for member in archive.infolist():
            name = safe_name(member.filename)
            if stat.S_ISLNK(member.external_attr >> 16):
                raise ValueError(f"Link in {path.name}: {name}")
            if member.flag_bits & 1:
                raise ValueError(f"Encrypted member in {path.name}: {name}")
            row = dict(path=name, directory=member.is_dir(), bytes=member.file_size,
                       packed_bytes=member.compress_size, crc32=f"{member.CRC:08x}")
            if not member.is_dir() and member.file_size <= 262144 and Path(name).suffix.lower() in (".txt", ".md"):
                data = archive.read(member)
                row["text"] = data.decode("utf-8-sig", errors="replace")
                row["text_sha256"] = hashlib.sha256(data).hexdigest()
            rows.append(row)
    return rows


def sevenzip_members(path):
    output = subprocess.run(["7z", "l", "-slt", str(path)], check=True,
                            capture_output=True, text=True).stdout
    listing = output.split("----------\n", 1)[1]
    rows = []
    for block in listing.strip().split("\n\n"):
        fields = dict(line.split(" = ", 1) for line in block.splitlines() if " = " in line)
        if "Path" not in fields:
            continue
        name = safe_name(fields["Path"])
        if fields.get("Encrypted") == "+" or any("Link" in key for key in fields):
            raise ValueError(f"Encrypted or linked member in {path.name}: {name}")
        rows.append(dict(path=name, directory=fields.get("Folder") == "+",
                         bytes=int(fields.get("Size", 0)), crc32=fields.get("CRC", "").lower()))
    if not rows:
        raise ValueError(f"Empty archive listing: {path}")
    return rows


def main():
    if OUTPUT.exists():
        raise FileExistsError(OUTPUT)
    archives = sorted(p for p in SOURCE.rglob("*") if p.suffix.lower() in (".zip", ".7z"))
    if not archives:
        raise ValueError("No source archives found")
    rows = []
    for index, path in enumerate(archives, 1):
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        members = zip_members(path) if path.suffix.lower() == ".zip" else sevenzip_members(path)
        files = [m for m in members if not m["directory"]]
        rows.append(dict(id=f"source-{index:02d}", archive=str(path.relative_to(SOURCE)),
                         sha256=digest, bytes=path.stat().st_size,
                         unpacked_bytes=sum(m["bytes"] for m in files), members=members))
    result = dict(schema=1, source=str(SOURCE), archives=rows,
                  archive_count=len(rows), archive_bytes=sum(r["bytes"] for r in rows),
                  unpacked_bytes=sum(r["unpacked_bytes"] for r in rows),
                  scope="Archive members only. Container asset and animation identities remain uninspected.")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("x") as stream:
        json.dump(result, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(json.dumps({key: value for key, value in result.items() if key != "archives"}, indent=2))
    for row in rows:
        print(row["id"], row["archive"], row["unpacked_bytes"], "bytes")
        for member in row["members"]:
            if not member["directory"]:
                print(" ", member["path"], member["bytes"])


if __name__ == "__main__":
    main()

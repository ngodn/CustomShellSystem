"""Extract inventoried animation sources separately and list their IoStore packages."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

from inventory_library import ROOT, safe_name

WORK = ROOT / "work/cine-animation-port"


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--retoc", type=Path, required=True)
    args = parser.parse_args()
    retoc = args.retoc.resolve(strict=True)
    inventory_path = WORK / "inventory.json"
    inventory = json.loads(inventory_path.read_text())
    source = Path(inventory["source"]).resolve(strict=True)
    destination = WORK / "sources"
    report_path = WORK / "extraction.json"
    if destination.exists() or report_path.exists():
        raise FileExistsError("Source extraction already exists; inspect it before retrying")
    if inventory["unpacked_bytes"] > 1024**3:
        raise ValueError("Unexpected source size; review storage before extraction")
    if shutil.disk_usage(WORK).free < inventory["unpacked_bytes"] + 1024**3:
        raise ValueError("Insufficient free space for this source batch")
    for archive in inventory["archives"]:
        path = source / safe_name(archive["archive"])
        if sha256(path) != archive["sha256"]:
            raise ValueError(f"Source changed: {path}")
    destination.mkdir()
    records = []
    for archive in inventory["archives"]:
        path = source / archive["archive"]
        output = destination / archive["id"]
        output.mkdir()
        files = []
        for member in archive["members"]:
            name = safe_name(member["path"])
            target = output / name
            if member["directory"]:
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            if path.suffix.lower() == ".zip":
                with zipfile.ZipFile(path) as zipped, zipped.open(name) as incoming, target.open("xb") as outgoing:
                    shutil.copyfileobj(incoming, outgoing, length=1024**2)
            else:
                with target.open("xb") as outgoing:
                    subprocess.run(["7z", "x", "-so", str(path), name], stdout=outgoing,
                                   stderr=subprocess.PIPE, check=True)
            if target.stat().st_size != member["bytes"]:
                raise ValueError(f"Extracted size differs: {target}")
            files.append(dict(path=name, bytes=target.stat().st_size, sha256=sha256(target)))
        containers = []
        for toc in sorted(output.rglob("*.utoc")):
            for suffix in (".pak", ".ucas"):
                if not toc.with_suffix(suffix).is_file():
                    raise ValueError(f"Missing container sibling: {toc.with_suffix(suffix)}")
            listing = subprocess.run([str(retoc), "list", str(toc), "--path"],
                                     capture_output=True, text=True, check=True)
            packages = ["../../../" + line.partition("../../../")[2]
                        for line in listing.stdout.splitlines()
                        if "../../../" in line and line.endswith(".uasset")]
            if not packages:
                raise ValueError(f"No assets listed: {toc}")
            containers.append(dict(path=str(toc.relative_to(output)), packages=packages,
                                   stdout=listing.stdout, stderr=listing.stderr))
        if sha256(path) != archive["sha256"]:
            raise ValueError(f"Source changed during extraction: {path}")
        record = dict(id=archive["id"], archive=archive["archive"],
                      archive_sha256=archive["sha256"], files=files, containers=containers)
        (output / "extraction.json").write_text(json.dumps(record, indent=2) + "\n")
        records.append(record)
        print(archive["id"], len(files), "files;", sum(len(c["packages"]) for c in containers), "packages", flush=True)
    groups = {}
    for record in records:
        for file in record["files"]:
            groups.setdefault(file["sha256"], []).append(dict(source=record["id"], path=file["path"]))
    report = dict(schema=1, inventory_sha256=sha256(inventory_path),
                  retoc=dict(path=str(retoc), sha256=sha256(retoc)), sources=records,
                  identical_files=[dict(sha256=key, members=members) for key, members in groups.items() if len(members) > 1],
                  scope="Extracted bytes and container paths only; animation types, tracks and duplicate motions not yet verified.")
    with report_path.open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")


if __name__ == "__main__":
    main()

"""Read each supplied animation container independently with the existing MeshExport tool."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess

from inventory_library import ROOT

WORK = ROOT / "work/cine-animation-port"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sources", nargs="+", required=True)
    parser.add_argument("--mode", choices=("metadata", "tracks", "audit"), default="metadata")
    parser.add_argument("--exporter", type=Path,
                        default=ROOT / "tools/MeshExport/bin/Release/net10.0/MeshExport.dll")
    parser.add_argument("--revision", default="", help="Separate outputs for a decoder revision")
    parser.add_argument("--package-list", type=Path,
                        help="Explicit referenced packages; requires one source and a revision")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{0,32}", args.revision):
        raise ValueError("Invalid decoder revision")
    if len(args.sources) != len(set(args.sources)):
        raise ValueError("Duplicate source selection")
    packages = None
    if args.package_list:
        if len(args.sources) != 1 or not args.revision:
            raise ValueError("Referenced packages require one source and a revision")
        packages = args.package_list.read_text().splitlines()
        if not packages or len(packages) != len(set(packages)) or any(
                not re.fullmatch(r"/Game/(?:[A-Za-z0-9_-]+/)*[A-Za-z0-9_-]+", p) for p in packages):
            raise ValueError("Invalid referenced package list")
    extraction = json.loads((WORK / "extraction.json").read_text())
    provenance = json.loads((ROOT / "work/anim1/provenance.json").read_text())
    game = Path(provenance["sb_index"]["path"]).parent.resolve(strict=True)
    mapping = Path(provenance["sb_mappings"]["path"]).resolve(strict=True)
    exporter = args.exporter.resolve(strict=True)
    if digest(mapping) != provenance["sb_mappings"]["sha256"]:
        raise ValueError("Retail mapping changed; inspect before decoding")
    records = {r["id"]: r for r in extraction["sources"]}
    environment = os.environ.copy()
    for key in list(environment):
        if key.startswith("CSS_"):
            del environment[key]
    environment["CSS_SOURCE_GAME"] = "GAME_StellarBlade"
    environment[{"metadata": "CSS_AUDIT_ONLY", "tracks": "CSS_SOURCE_TRACKS",
                 "audit": "CSS_TRACK_AUDIT"}[args.mode]] = "1"
    for identity in args.sources:
        source = records[identity]
        if len(source["containers"]) != 1:
            raise ValueError(f"Expected one container: {identity}")
        mount = WORK / "mounts" / identity
        container = source["containers"][0]
        toc = WORK / "sources" / identity / container["path"]
        links = [p for p in game.iterdir() if p.is_file() and p.suffix in (".pak", ".utoc", ".ucas")]
        if not any(p.name == "global.utoc" for p in links):
            raise ValueError("Missing global source container")
        links += [toc.with_suffix(suffix) for suffix in (".pak", ".utoc", ".ucas")]
        if len({p.name for p in links}) != len(links):
            raise ValueError("Container filename collision")
        mount.mkdir(parents=True, exist_ok=True)
        for path in links:
            target = mount / path.name
            if target.is_symlink():
                if target.resolve() != path.resolve():
                    raise ValueError(f"Unexpected existing mount: {target}")
            elif target.exists():
                raise ValueError(f"Expected a symlink: {target}")
            else:
                target.symlink_to(path.resolve(strict=True))
        if {p.name for p in mount.iterdir()} != {p.name for p in links}:
            raise ValueError("Unexpected extra files in isolated mount")
        category = args.mode + ("-" + args.revision if args.revision else "")
        destination = WORK / category / identity
        destination.mkdir(parents=True, exist_ok=False)
        results = []
        assets = packages or ["/Game/" + p.split("/SB/Content/", 1)[1].removesuffix(".uasset")
                              for p in container["packages"]]
        for index, asset in enumerate(assets):
            if args.mode != "metadata":
                metadata_category = "metadata-" + args.revision if packages else "metadata"
                metadata = WORK / metadata_category / identity / f"asset-{index:03d}" / "source-mesh.json"
                if not metadata.is_file():
                    results.append(dict(asset=asset, status="metadata_unavailable"))
                    continue
                kind = json.loads(metadata.read_text())["Type"]
                if kind != "AnimSequence":
                    results.append(dict(asset=asset, type=kind, status="non_animation_dependency"))
                    continue
            output = destination / f"asset-{index:03d}"
            log = destination / f"asset-{index:03d}.log"
            command = ["dotnet", str(exporter), str(mount), str(mapping), asset, str(output)]
            with log.open("xb") as stream:
                result = subprocess.run(command, env=environment, stdout=stream, stderr=subprocess.STDOUT,
                                        timeout=300)
            row = dict(asset=asset, output=str(output.relative_to(WORK)), exit_code=result.returncode)
            metadata = output / "source-mesh.json"
            if metadata.exists():
                row["metadata_sha256"] = digest(metadata)
                document = json.loads(metadata.read_text())
                row["type"] = document.get("Type") if isinstance(document, dict) else None
            results.append(row)
            print(identity, args.mode, asset, result.returncode, row.get("type"), flush=True)
            (destination / "progress.json").write_text(json.dumps(results, indent=2) + "\n")
        report = dict(source=identity, mode=args.mode, exporter_sha256=digest(exporter),
                      mappings_sha256=digest(mapping), results=results)
        if args.package_list:
            report["package_list_sha256"] = digest(args.package_list)
        (destination / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()

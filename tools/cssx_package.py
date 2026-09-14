#!/usr/bin/env python3
"""Validate and package a CSSX extension without running its gameplay code."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

REPO = Path(__file__).resolve().parents[1]
RESERVED = {"con", "prn", "aux", "nul"} | {
    f"{prefix}{number}" for prefix in ("com", "lpt") for number in range(10)
}
GENERATED = {"state", "logs", "output", "cache", "runtime", ".git", "build"}


def package_path(value: str) -> PurePosixPath:
    if not isinstance(value, str) or not value or len(value) > 512:
        raise ValueError("Package file paths must be nonempty relative strings")
    if "\\" in value or ":" in value or value.startswith("/"):
        raise ValueError(f"Invalid package path: {value!r}")
    for part in value.split("/"):
        if (
            part in ("", ".", "..")
            or part.endswith((".", " "))
            or part.split(".", 1)[0].lower() in RESERVED
            or any(ord(c) < 32 or c in '<>\"|?*' for c in part)
        ):
            raise ValueError(f"Invalid Windows package path: {value!r}")
    path = PurePosixPath(value)
    if path.parts[0].lower() in GENERATED:
        raise ValueError(f"Generated or development files cannot ship: {value!r}")
    return path


def source_file(root: Path, value: str) -> Path:
    relative = package_path(value)
    path = root.joinpath(*relative.parts)
    if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(root):
        raise ValueError(f"Missing file or path outside extension: {value}")
    return path


def validate_pe(path: Path) -> None:
    with path.open("rb") as stream:
        header = stream.read(64)
        if len(header) != 64 or header[:2] != b"MZ":
            raise ValueError(f"Expected a Windows DLL: {path.name}")
        offset = int.from_bytes(header[60:64], "little")
        if offset > path.stat().st_size - 24:
            raise ValueError("Invalid DLL header offset")
        stream.seek(offset)
        pe = stream.read(24)
        if pe[:6] != b"PE\0\0\x64\x86" or not int.from_bytes(pe[22:24], "little") & 0x2000:
            raise ValueError("Native extensions require an x64 Windows DLL")


def safe_label(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_-]+", "", value) or "Extension"


def build(source: Path, output: Path, validator: Path, template: str,
          compile_pkl: bool = False, pkl: str = "pkl") -> Path:
    source = source.resolve(strict=True)
    metadata = source_file(source, "extension.json")
    if metadata.stat().st_size > 65536:
        raise ValueError("Manifest exceeds 64 KiB")
    manifest = json.loads(metadata.read_text(encoding="utf-8"))
    files = ["extension.json", manifest["entry"]]
    files += [manifest[key] for key in ("banner", "menu") if key in manifest]
    extras = manifest.get("files", [])
    if not isinstance(extras, list) or len(extras) > 512:
        raise ValueError("Manifest files must be an array of at most 512 relative filenames")
    files += extras
    for name in ("LICENSE", "LICENSE.txt", "CREDITS.txt"):
        if (source / name).is_file():
            files.append(name)
    files = list(dict.fromkeys(files))
    if len({name.casefold() for name in files}) != len(files):
        raise ValueError("Package filenames collide on Windows")
    if not validator.is_file():
        raise ValueError("Build cssx_validate first, or provide --validator /path/to/cssx_validate")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="cssx-package-") as temporary:
        stage = Path(temporary) / "extension"
        stage.mkdir()
        menu = manifest.get("menu")
        for name in files:
            relative = package_path(name)
            destination = stage.joinpath(*relative.parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            if compile_pkl and name == menu:
                pkl_source = source_file(source, str(relative.with_suffix(".pkl")))
                subprocess.run([
                    pkl, "eval", "--no-project", "--allowed-modules=pkl:,file:",
                    "--allowed-resources=^prop:pkl\\.outputFormat$", "--timeout=30", "--format=json",
                    "--output-path", str(destination), str(pkl_source),
                ], check=True, timeout=40)
            else:
                shutil.copyfile(source_file(source, name), destination)
        if compile_pkl and not menu:
            raise ValueError("--compile-pkl requires a manifest menu path")
        if manifest["kind"] == "native":
            validate_pe(stage / manifest["entry"])
        subprocess.run([str(validator.resolve()), str(stage)], check=True, capture_output=True, text=True)
        folder = manifest["id"]
        filename = template.format(name=safe_label(manifest["title"]),
                                   author=safe_label(manifest["author"]),
                                   version=safe_label(manifest["version"]), id=folder)
        if package_path(filename).name != filename:
            raise ValueError("ZIP name must be a filename, without directories")
        if not filename.endswith(".zip"):
            filename += ".zip"
        destination = output / filename
        pending = Path(temporary) / "package.zip"
        with zipfile.ZipFile(pending, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name in sorted(files):
                info = zipfile.ZipInfo(f"{folder}/{name}", (2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, (stage / name).read_bytes())
        with zipfile.ZipFile(pending) as archive:
            bad = archive.testzip()
            if bad:
                raise ValueError(f"ZIP verification failed: {bad}")
        # The temporary output lives beside the destination for atomic replacement.
        with tempfile.NamedTemporaryFile(dir=output, prefix=".cssx-", suffix=".tmp", delete=False) as stream:
            staged_zip = Path(stream.name)
        try:
            shutil.copyfile(pending, staged_zip)
            staged_zip.replace(destination)
        finally:
            staged_zip.unlink(missing_ok=True)
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path, default=REPO / "dist/extensions")
    parser.add_argument("--validator", type=Path, default=REPO / "build/cssx-host/cssx_validate")
    parser.add_argument("--name", default="CSSX_{name}_{author}", help="ZIP template: {name}, {author}, {version}, {id}")
    parser.add_argument("--compile-pkl", action="store_true")
    parser.add_argument("--pkl", default="pkl", help="Pkl executable (only used with --compile-pkl)")
    args = parser.parse_args()
    try:
        result = build(args.source, args.output, args.validator, args.name, args.compile_pkl, args.pkl)
    except (ValueError, KeyError, OSError, subprocess.SubprocessError) as error:
        detail = error.stderr if isinstance(error, subprocess.CalledProcessError) and error.stderr else str(error)
        parser.exit(1, f"CSSX packaging failed: {detail}\n")
    print(result)
    with result.open("rb") as stream:
        print("SHA256:", hashlib.file_digest(stream, "sha256").hexdigest())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Fetch CINE's pinned BtbN LGPL encoder. Python 3.14, standard library only."""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import tempfile
import urllib.request
import zipfile

RELEASE = "autobuild-2026-10-26-14-25"
PACKAGE = "ffmpeg-n9.0.2-36-g6882969ce9-win64-lgpl-9.0"
URL = f"https://github.com/BtbN/FFmpeg-Builds/releases/download/{RELEASE}/{PACKAGE}.zip"
SHA256 = "6005d82061921faee9a6bfe22861c70e842e88bedf32b05f867180347944c8a2"
EXE_SHA256 = "6324bed4128c25a00e3d0e2280661c9d2e6f475f6f856e94a6860de71401a19d"
FFMPEG_COMMIT = "6882969ce9fd2aa6aa1d0008b2bfe1c838aac11d"
BUILD_COMMIT = "b4a2b1d7cc241786eb8e2b588a7ade40cad48bbd"
LICENSES = {
    "COPYING.GPLv3": "8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903",
    "COPYING.LGPLv3": "da7eabb7bafdf7d3ae5e9f223aa5bdc1eece45ac569dc21b3b037520b4464768",
}
CINE = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def download(url: str, destination: Path, expected: str, limit: int) -> None:
    request = urllib.request.Request(url, headers={"User-Agent": "CINE-ffmpeg-fetch/1"})
    with urllib.request.urlopen(request, timeout=60) as response, destination.open("wb") as out:
        count = 0
        while block := response.read(1024 * 1024):
            count += len(block)
            if count > limit:
                raise ValueError(f"Download exceeded size limit: {url}")
            out.write(block)
    if digest(destination) != expected:
        raise ValueError(f"SHA-256 mismatch: {url}")


def source_notice() -> str:
    return f"""FFmpeg for CINE

Unmodified official BtbN win64 LGPL static build, release {RELEASE}.
Binary archive: {URL}
Archive SHA-256: {SHA256}
ffmpeg.exe SHA-256: {EXE_SHA256}

FFmpeg is a separate executable, not linked into CINE. You may replace it
with your own compatible ffmpeg.exe. No source changes were made by CINE.
The build uses LGPLv3 (including GPLv3 terms). See LICENSE.txt,
COPYING.LGPLv3 and COPYING.GPLv3 beside this file.

Corresponding source directions (GPLv3 section 6(d))

Exact FFmpeg source:
https://github.com/FFmpeg/FFmpeg/tree/{FFMPEG_COMMIT}
Download:
https://github.com/FFmpeg/FFmpeg/archive/{FFMPEG_COMMIT}.tar.gz

Exact BtbN build scripts, patches and dependency source pins:
https://github.com/BtbN/FFmpeg-Builds/tree/{BUILD_COMMIT}
Download:
https://github.com/BtbN/FFmpeg-Builds/archive/{BUILD_COMMIT}.tar.gz
Each scripts.d file contains the source repository/archive URL and pinned
revision/hash for its dependency. Fetch those sources at those revisions,
including recursive submodules, with this checkout's download.sh. The
variant is win64 lgpl with add-in 9.0. Keep the FFmpeg checkout at
{FFMPEG_COMMIT}, rather than the moving release branch.
Follow that checkout's README/build.sh for the Docker build environment.
Run ffmpeg.exe -buildconf to see the embedded configure options.

Upstream source retrieval guidance:
https://github.com/BtbN/FFmpeg-Builds/issues/678
https://github.com/BtbN/FFmpeg-Builds/issues/661
The pinned scripts and source repositories, not a link to latest, identify
this binary's sources. CINE distributors remain responsible for keeping
these exact sources available for as long as required by section 6(d).
Archive/mirror the pinned sources and dependency notices with the release.
BtbN deletes daily binary assets after 14 days. This script never silently
replaces a missing pin with a newer build.
"""


def fetch(archive: Path | None = None) -> None:
    destination = CINE / "ffmpeg"
    destination.mkdir(parents=True, exist_ok=True)
    # Stage on the destination filesystem. Existing files survive failed verification.
    with tempfile.TemporaryDirectory(prefix=".fetch-", dir=destination) as temp:
        stage = Path(temp)
        if archive is None:
            archive = stage / "ffmpeg.zip"
            download(URL, archive, SHA256, 256 * 1024 * 1024)
        elif digest(archive) != SHA256:
            raise ValueError("Local archive SHA-256 does not match the pin")
        with zipfile.ZipFile(archive) as bundle:
            for member, filename, expected in (
                (f"{PACKAGE}/bin/ffmpeg.exe", "ffmpeg.exe", EXE_SHA256),
                (f"{PACKAGE}/LICENSE.txt", "LICENSE.txt", LICENSES["COPYING.LGPLv3"]),
            ):
                info = bundle.getinfo(member)
                if info.file_size > 150 * 1024 * 1024:
                    raise ValueError(f"Oversized archive member: {member}")
                with bundle.open(info) as source, (stage / filename).open("wb") as out:
                    shutil.copyfileobj(source, out)
                if digest(stage / filename) != expected:
                    raise ValueError(f"Archive member SHA-256 mismatch: {member}")
        for filename, expected in LICENSES.items():
            download(f"https://raw.githubusercontent.com/FFmpeg/FFmpeg/{FFMPEG_COMMIT}/{filename}",
                     stage / filename, expected, 128 * 1024)
        (stage / "SOURCE.txt").write_text(source_notice(), encoding="utf-8", newline="\n")
        # Publish the executable last, only after all licences and notices are ready.
        for filename in ("LICENSE.txt", *LICENSES, "SOURCE.txt", "ffmpeg.exe"):
            os.replace(stage / filename, destination / filename)
    print(f"Pinned ffmpeg.exe: {destination / 'ffmpeg.exe'} ({(destination / 'ffmpeg.exe').stat().st_size:,} bytes)")
    print(f"SHA-256: {digest(destination / 'ffmpeg.exe')}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, help="use a previously downloaded, hash-verified pinned archive")
    args = parser.parse_args()
    fetch(args.archive)


if __name__ == "__main__":
    main()

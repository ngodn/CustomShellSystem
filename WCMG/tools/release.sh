#!/usr/bin/env bash
# Builds the Windows exe and writes dist/wcmg-v<version>/MSII-WCMG-v<version>.zip
# with its sha256. Nexus takes archives, never a bare exe.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
version="$(head -1 "$here/VERSION")"
out="$here/dist/wcmg-v$version"
stage="$out/MSII-WCMG-v$version"
cmake -S "$here" -B "$here/build/windows" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$here/../native/toolchain-clang-cl.cmake" >/dev/null
cmake --build "$here/build/windows" --target wcmg wcmg_cli
rm -rf "$out"; mkdir -p "$stage"
cp "$here/build/windows/WCMG.exe" "$stage/"
cp "$here/README.md" "$stage/README.md"
cp "$here/THIRD_PARTY_NOTICES.txt" "$stage/"
cp "$here/assets/wcmg-meme-1280.png" "$stage/wcmg.png"
cp "$here/docs/nexus-changelog-v$version.bbcode" "$out/changelog.bbcode" 2>/dev/null || true
cp "$here/docs/nexus-description.bbcode" "$out/description.bbcode" 2>/dev/null || true
cp "$here/assets/wcmg-meme-1280.png" "$out/"
( cd "$out" && rm -f "MSII-WCMG-v$version.zip" && zip -qr "MSII-WCMG-v$version.zip" "MSII-WCMG-v$version" && sha256sum "MSII-WCMG-v$version.zip" > "MSII-WCMG-v$version.zip.sha256" )
echo "release: $out/MSII-WCMG-v$version.zip"
cat "$out/MSII-WCMG-v$version.zip.sha256"

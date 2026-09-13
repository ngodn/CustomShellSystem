# Runtime releases

The release ZIP contains a single `CustomShellSystem/` folder to extract into
`MortalShell2/Binaries/Win64/ue4ss/Mods/`. Outfit packages and UE4SS are separate.
The canonical version is the root `VERSION` file. CMake generates the loader's
version from it. Tags use `vX.Y.Z`; GitHub release titles use `MSII - CSS vX.Y.Z`.

## Build

Commit the intended source, update VERSION, and create an annotated version tag.
Build from a clean checkout at that tag:

```sh
python3 tools/css_release.py build --sdk /absolute/path/to/ue4ss-sdk-d7e7826d
python3 tools/css_release.py verify dist/releases/MSII-CSS-v0.1.1.zip
```

The script builds both Release DLLs from that checkout and uses an explicit
seven-file payload allowlist plus generated release metadata. It never copies
from the installed mod. The only interface image needed is wardrobe-v1.png;
old outfit seals and developer catalogs are excluded. One loader and one core
ship, with core.json pointing to that core. No state, backups, requests, cache,
logs, diagnostics, game content, SDK, import libraries or debug symbols ship.
Third-party notices and short installation instructions accompany the runtime.
The generated manifest records version, source revision and payload hashes.
The ZIP checksum is written beside it. Existing release archives are not overwritten.

## Fresh-start verification

`css_startup_tests` calls the production state loader, catalog discovery and
atomic writer. It starts with no state or catalog folder, verifies default-file
creation, settings persistence, backup recovery, and regeneration after deletion.
With a pak fixture directory, it checks package discovery and thumbnail/cache
creation, deletes the cache and confirms regeneration. It does not mutate the
installed mod or the game save.

```sh
cmake -S native -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
python3 -m unittest discover -s tests -p 'test_release.py'
# Extract the finished ZIP to a temporary directory first:
build/host/css_startup_tests /temporary/extracted/CustomShellSystem /path/to/Paks/~mods
```

The extra test files belong only to the temporary extraction. Reverify the
original ZIP afterwards. These checks cover production persistence/discovery
code and archive cleanliness. A new in-game cold start of the exact ZIP is a
separate check, not something a host test establishes.

## 0.1.1 scope

Includes the confirmed inventory material fix and first-run initialization fixes.
The working-tree inventory-derived lighting experiment is excluded because its
visual behavior has not been confirmed. The known wardrobe lighting difference
remains in the user-facing notes. No authored or converted outfit is bundled.

GitHub publishing was deferred by the user on 2026-09-14 after authentication
failed. The prepared title is `MSII - CSS v0.1.1`; notes are in
`packaging/release-notes.md`. A local ZIP and tag do not imply publication.

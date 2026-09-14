# Package discovery and diagnostics

CSS 0.2.1 searches the game's `Content/Paks` directory recursively. The recommended install location remains `Content/Paks/~mods/<package>/`, but CSS also finds packages placed directly in `Content/Paks` or another subfolder there. Keep each package's `.pak`, `.utoc` and `.ucas` together, with their original filenames, and restart after installing containers.

This follows the project-content search in UE 5.6.1's `FPakPlatformFile::GetPakFolders` and recursive traversal in `FindPakFilesInDirectory` (`Engine/Source/Runtime/PakFile/Private/IPlatformFilePak.cpp`). Epic documents those functions in the [FPakPlatformFile API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/PakFile/FPakPlatformFile?application_version=5.6). CSS does not currently search the engine-content or project-saved pak folders.

## Failure handling

A damaged or incomplete package no longer aborts discovery of healthy packages. CSS rejects that package and records its filename and reason. Duplicate outfit IDs are reported; the first valid package in sorted path order is retained. Remove duplicate installations rather than relying on that order, since Unreal still mounts the containers independently.

Checksum, metadata, material-reference and color-resource validation remain enabled. CSS does not apply a rejected outfit or change its asset files. A cache write failure rejects the affected package rather than exposing an outfit with missing resources. Loose developer catalogs remain strict.

Directory errors are recorded individually. Directory symlinks are not followed. The scan remains bounded to 1,024 pak files and reports when it reaches the limit.

`CSS.log` records the searched folder, scanned count, and each pak's outcome. `runtime/status.json` includes the same information under `catalog`:

- `loaded`: package manifests admitted to the catalog.
- `rejected`: packages that failed validation or caching, with a `reason`.
- `ignored`: pak files without supported CSS.Package metadata, including ordinary game and replacement packages.
- `errors`: folder traversal failures.

An empty menu gives different guidance for rejected packages, unreadable folders and missing CSS metadata. File discovery is separate from successful engine mounting and in-game asset loading. A discovered entry alone does not prove that its mesh or animation works.

## Windows paths

State and backup paths remain native filesystem paths. Image-cache keys and diagnostic text use explicit UTF-8. Passing a non-ASCII installation path through `std::filesystem::path::string()` could throw `No Unicode translation` on Windows; the startup and backup recovery tests reproduce that failure before the fix and pass afterward.

This does not claim that every Windows path-length restriction is removed. Windows has separate [long-path requirements](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation). The Windows test executable passed a long-path fixture under Wine, which is not proof of identical behavior on a Windows installation.

## Regression checks

Host tests:

```sh
cmake -S native -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/host -j4
ctest --test-dir build/host --output-on-failure
build/host/css_discovery_tests /path/to/extracted/CSS_OUTFIT_P
build/host/css_package_tests /path/to/extracted/CSS_OUTFIT_P
```

`css_discovery_tests` copies the supplied outfit into an isolated temporary directory. It tests a damaged neighbor, missing companion, duplicate ID, unwritable cache path, unrelated pak, root-folder placement, uppercase extension and missing folder. It leaves the supplied package unchanged. `css_package_tests` checks metadata, cached thumbnails and masks, cache reuse/repair and corruption rejection.

The same tests can be built for Windows without UE4SS:

```sh
cmake -S native -B build/windows-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCSS_DATA_TESTS_ONLY=ON \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/native/toolchain-clang-cl.cmake"
cmake --build build/windows-tests -j4
```

Run the resulting executables on Windows or in an isolated Wine prefix. `css_startup_tests` covers fresh settings, backups, corrupt-state recovery and Unicode paths. Its optional arguments are a fresh writable mod directory and an installed package directory.

## September 14 support reports

The local reproductions establish CSS bugs in package failure isolation, search scope and Windows path conversion. They do not establish which caused each Nexus report. The supplied Irmassidarkstar log was from 0.1.2; no updated 0.2.0 log accompanied the later comments. Their UE4SS revision matched the tested ABI, so there was no evidence to recommend another UE4SS reinstall.

The nine local outfit packages, including all four port bundles and Seductress V2, passed archive/container checks, native catalog loading and thumbnail/color-cache validation across 28 variant entries. Those checks do not replace visual and gameplay testing on the affected users' installations.

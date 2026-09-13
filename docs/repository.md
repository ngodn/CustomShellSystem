# Repository conventions

The repository tracks CSS source, tests, documentation, interface artwork,
portrait examples, material recipes and dependency patches. `main` is the local
development branch. Commit each coherent change separately, with a short,
imperative subject describing its result. Check the staged diff before committing.

Build products, downloaded dependencies, extracted game content, converted
packages, backups, live state and diagnostic evidence stay outside Git through
`.gitignore`. Use `dist/` for distributable package artifacts and `work/` for
investigation output. These directories remain on disk. Do not force-add them.
Track a dependency patch and its source revision when modifying an ignored
reference checkout. Keep upstream licenses with vendored code.

CSS uses C++23, Python 3.14 and .NET 10 for the optional mesh exporter. The native
build requires the exact local UE4SS SDK described in [SDK notes](ue4ss-sdk.md),
clang-cl 22.1.8 and the xwin headers/import libraries. The SDK is not included in
Git. Its upstream Unreal dependency requires authorized GitHub access. A fresh
clone therefore needs local dependency setup before building the Windows DLL.
No license for redistributing third-party mod content is granted by this repo.

## Portrait and packaging dependencies

- CUE4Parse: `bbb3551af4c0c1bf481bc2821a18bd3b72706ff4` from
  [FabianFG/CUE4Parse](https://github.com/FabianFG/CUE4Parse), in
  `reference/CUE4Parse`.
- Apply `patches/cue4parse-linux-export.patch` with
  `git -C reference/CUE4Parse apply ../../patches/cue4parse-linux-export.patch`.
  This preserves Linux path separators and pins the patched Bcl.Memory dependency.
- repak: `355b5f62f51959c7cc6dd5a51708646ef483065d` from
  [trumank/repak](https://github.com/trumank/repak), in `reference/repak`.
- The converter accepts explicit `--retoc`, `--repak` and `--game` paths. Its
  current defaults point to this development machine. Package conversion audits
  record the actual retoc and repak binary hashes.
- Color-mask authoring uses Python 3.14, Pillow 12.3 and NumPy 2.5. The
  generator reads extracted original textures; derived masks stay under `work/`.
- Portrait rendering uses Blender's `bpy` 4.5.13 under a separate Python 3.11
  environment. See [the package guide](css-packages.md) for the rendering pipeline.

The tracked portrait examples depict the original mods and retain their author
credits in the package guide. The older generated seal artwork in `assets/`
belongs to the initial UI prototype. Normal outfits supply their own thumbnails.

## Local checks

```sh
cmake -S native -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
git diff --check
git diff --cached --check
```

Some conversion tests need the ignored local game fixtures and packages, and
skip when these are absent. Live preview checks require a running game. A clean
Git status alone does not establish game compatibility.

Ignore matching follows [Git's documented rules](https://git-scm.com/docs/gitignore).
Use `git check-ignore -v PATH` to explain why a local file is excluded.

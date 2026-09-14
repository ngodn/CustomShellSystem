# Packaging tool setup

Run the Python tools from a CSS source checkout. Use Python 3.14; the tested runtime is 3.14.7. With mise, inspect `mise current` and select the project runtime before building. Packaging uses the Python standard library. Optional mask generators use Pillow/NumPy; Blender portrait/export scripts have their own `bpy` environment. Neither is needed to package finished containers.

## Retoc and repak

The tested retoc base is `d7b635039c3db60942efabcd29d49679f42ab089`, with this repository's [case-handling/logging patch](../../patches/retoc-css-import-case.patch). Repak is `355b5f62f51959c7cc6dd5a51708646ef483065d` (v0.2.3). Upstream sources: [retoc](https://github.com/trumank/retoc), [repak](https://github.com/trumank/repak).

With Git and a compatible Rust/Cargo toolchain installed, these commands create isolated local tool checkouts. Run them only for new, empty destinations:

```sh
git clone https://github.com/trumank/retoc.git build/retoc-author
git -C build/retoc-author checkout d7b635039c3db60942efabcd29d49679f42ab089
git -C build/retoc-author apply ../../patches/retoc-css-import-case.patch
cargo build --release --locked --manifest-path build/retoc-author/Cargo.toml --package retoc_cli

git clone https://github.com/trumank/repak.git build/repak-author
git -C build/repak-author checkout 355b5f62f51959c7cc6dd5a51708646ef483065d
cargo build --release --locked --manifest-path build/repak-author/Cargo.toml --package repak_cli
```

On Linux the executable paths are `build/retoc-author/target/release/retoc` and `build/repak-author/target/release/repak`. Supply those paths to the project builder; the audit records the exact binary hashes. Upstream changes need another conversion check, not an assumption that “latest” is equivalent.

The older `css_convert.py` and `css_package.py install` defaults reference the original development machine. Supply `--game`, `--retoc` and `--repak` explicitly. The new project builder requires those arguments and never relies on those defaults.

## Game inputs and host limitations

`--game` points to the **MortalShell2 directory containing Content/Paks**, not `Sparta`, `~mods` or UE4SS's Mods directory. Conversion resolves imports against the installed base `global` and `pakchunk` containers. Keep those intact. Do not copy them into the release ZIP.

The end-to-end conversion host tested here is Linux. Retoc and repak are separate programs; select binaries for the host running Python. The converter stages symbolic links, including directory links during verification. Native Windows Python therefore needs working symlink privileges and a compatible filesystem. Native Windows conversion has not been verified as a complete workflow. The optional installer also uses the Linux `/proc` process check, so it is not a general Windows installer. Windows players can install the finished ZIP manually.

A Unix conversion host does not imply a Linux target asset cook. The game consumes compatible Windows cooked assets. [Asset authoring](asset-authoring.md) covers that distinction.

# Custom Combat System

CCS is a standalone UE4SS C++23 mod under development for Mortal Shell II, UE 5.6.1. The default build is a passive foundation. It does not change combat or display the unfinished menu prototype.

[Takeover review](docs/takeover-review.md) records the defects found in Gemini's implementation, fixes, validation, and remaining work. [UI reference](docs/ui-reference.md) records the user's latest layout. [Work queue](docs/work-queue.md) contains the next implementation steps.

Runtime initialization no longer loads `data/catalog.json`. That file is retained only for research and regression tests. The experimental menu reads current player selector references through [bounded loaded-move discovery](docs/loaded-moves-runtime.md). Observed references remain ineligible for assignment until their slot roles and combat compatibility are verified. Owned Tarstones and enemy moves remain unavailable.

Full [live game discovery with patch-aware validation](docs/runtime-discovery.md) remains under development. The first [loaded combat discovery probe](docs/discovery-probe-01.md) verified current Scythe selectors. [Registry controls](docs/discovery-probe-02.md) await live validation. Installed-content identity and metadata-cache invalidation remain unfinished.

Build and test from the CustomShellSystem repository root:

```sh
cmake -S CCS -B CCS/build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build CCS/build/host -j 4
ctest --test-dir CCS/build/host --output-on-failure
python3 CCS/tools/ccs.py build
```

The Windows build uses the shared clang-cl/xwin toolchain and pinned UE4SS SDK/import library. The compiled mod does not depend on CSS or CSSX at runtime. `CCS_EXPERIMENTAL_MENU` defaults to OFF; the menu prototype remains unverified and should not be staged for normal play.

To regenerate metadata when the retained exports change:

```sh
python3 CCS/work/takeover/generate_catalog.py
python3 CCS/work/takeover/generate_catalog.py --check
```

Staging requires a fresh confirmation that the user stopped playing, a closed game, and the pinned UE4SS DLL. The tool verifies copied files, replaces the selector atomically, and preserves existing presets. Build success alone does not establish safe gameplay or FPS behavior.

Experimental menu file actions use a [bounded asynchronous worker](docs/persistence-runtime.md). The [menu retains pooled widgets](docs/menu-pooling.md), including a [preset name field and save/delete confirmations](docs/preset-menu-runtime.md). Naming and confirmation code is implemented but not clicked through in game. Native glyphs, pointer/focus validation, final widget styling and live item UI still need completion before activation.

Optional [diagnostic profiling](docs/frame-profiling.md) records bounded loader/core captures and writes reports on a worker. It is disabled in normal builds. Live baseline and CSS/CSSX coexistence comparisons remain pending.

A [loader-owned native pre-hook service](docs/native-hook-service.md) now guards callback/core lifetimes. The separate [read-only montage-task probe](docs/discovery-probe-03.md) is compiled locally and registers a bounded observation hook only when armed. It is not installed or live verified. Montage replacement remains unavailable. New candidates require their rebuilt loader because the host context gained a checked capability.

The experimental menu also has [background installation change monitoring](docs/content-monitor.md). It pauses observations on inspection failure and requires restart after changed game/package metadata. This is not a mounted-content or native compatibility certificate; full patch-aware discovery/cache validation remains unfinished.

# CSS research index

For outfit authors, start with the [modding guide](modding/README.md). It links the authoring requirements, project utility, color recipes, package contract and release checks. The [source research](modding-research.md) separates official Unreal behavior from local verification.

## Established findings

- [Next-Gen takeover status](../../CSS-Mod-Authoring/docs/next-gen-status.md) is the current roadmap for `nextgen100`. The beta controls have been integrated with 0.4.2 recovery/HUD changes; SeduXtress materials and package authoring still require repair. [Evidence](../../CSS-Mod-Authoring/docs/next-gen-baseline-findings.md).

- [Native Inventory CSS](inventory-ui-development.md) is the current interface in development. Four-page navigation, native display retention, mouse controls and startup have been checked locally. It replaces the standalone N wardrobe. Older preview and camera findings below are historical.

- [Color customization](colors.md) documents per-part palettes/sliders, self-contained dye resources, exact Original reset, and the verified game-specific mipmap adapter.
- [Color convention](control-convention.md) is the standard every package should follow: control groups and roles, hue locking, the palette rules, and how a custom color, a palette and a group tint combine.

- CSS.Package v1 embeds metadata and author artwork in the pak. IoStore assets use isolated package namespaces. [Package guide](css-packages.md) documents the converter, verification and installation.
- All three `_P` packages are installed and discovered in-game. HIT2 gameplay was confirmed by the user. The paused-preview cloth defect is fixed by enabling the separate cloth tick on the disposable visual copy. [Cloth findings](preview-cloth.md).

- This game uses UE5.6 and the supplied UE4SS 97b7e501 Game/Shipping/Win64 runtime. The import library and installer are pinned to its exact DLL hash; retained headers remain d7e7826d. See [SDK notes](ue4ss-sdk.md) for the migration and verification limits. Native consumer code is C++23, clang-cl/MSVC ABI, release dynamic CRT.
- The prior attack failure was a stranded completed-prologue interaction. CSS does not call shell switch, ability, save-game or unlock functions.
- All three Beaute body meshes use the original human skeleton. Preserve that reference.
- Core reload uses a permanent loader with serialized engine/UI callbacks. The core never registers a callback or starts a thread.
- State belongs under the CSS mod's state directory, separate from game saves.
- Cross-shell material holes came from the old component's material overrides. Clear overrides when equipping a different appearance and restore the captured original material paths on removal.
- Global pause needs a separate animated visual copy. A matching non-additive idle driven with `SetPosition(..., false)` animates that copy while world time stays frozen. Do not replace the real player's animation mode for this.
- CameraStateFramework's `ActiveCameraActor` is the effective view target in this game. Preserve the existing camera across wardrobe refreshes. The final user preference is conditional default recentering and vertical-only right-stick inversion.

## Rejected approaches

- Converting Beaute packs with only global containers silently produces `/Engine/UnknownPackage` and `UnknownExport` imports. Container verification and identical export bytes do not catch it. Include all base containers during conversion and reject unresolved imports explicitly.
- Loading new IoStore packs or a new permanent native loader requires an initial game launch. Core reload does not imply hot replacement of mounted asset containers.
- Do not build against a different UE4SS commit or reuse Stellar Blade engine offsets.
- Do not ship CNS Lua, UI assets, or game-specific files as CSS. CSS independently implements the feature ideas.
- `A_Genessa_Idle_H` is an additive sequence, not a standalone idle. Live reflection established `AdditiveAnimType=1`; the working preview uses `A_Shared_Idle_L`.
- Do not force material-array resizing through SDK allocator exports absent from the installed runtime. Reflected `SetMaterial` calls clear and restore existing slots without that ABI dependency.

| Artifact | Takeaway |
| --- | --- |
| [STATUS.md](STATUS.md) | Current implementation and verification status |
| [Next-Gen Architecture](next-gen-customization-architecture.md) | CSS 2.0 architecture: UI kit, templates vs profiles, Kawaii physics, and Stellar Blade jiggle |
| [Inventory integration feasibility](inventory-integration-feasibility.md) | Live tab indices, native display, layout and bounded prototype plan |
| [Inventory integration primary sources](inventory-integration-primary-sources.md) | UMG reorder limits, focus/input ownership and pinned native API evidence |
| [animated-preview.md](animated-preview.md) | Paused-world animation, camera refresh behavior, live cleanup evidence and rejected additive idle |
| [README](../README.md) | User controls, independent state, installation and native reload workflow |
| [native-css-research.md](native-css-research.md) | Primary sources and exact native API research |
| [SDK-NOTES.md](../reference/ue4ss-sdk-d7e7826d/SDK-NOTES.md) | Exact local SDK, dependencies, hashes and build settings |
| [conversion manifest](../local-packs/conversion-manifest.json) | Ten relocated assets, source hashes and unchanged payload hashes |
| [prior investigation](../../investigation/2026-09-13/findings.md) | Attack-state recovery and live diagnostic evidence |

- [Beacon return recovery](beacon-transition-recovery.md): same-player mesh resets, deferred restoration, menu ownership and live regression checks.

- [Mortal Shell II combat system](ms2-combat-system.md): weapons, seals, sidearms, shells and Tarstones with numbers read from the installed build's item definitions, weapon actors, attribute tables and Tarforge curves, cross-checked against patch notes and guides.
- [Auto-combos](ms2-loadout-combos.md): follow-up chains that fire after parry, perfect guard, guard hit or harden, with the trigger tags, chainable input tags, Resolve and cooldown constraints, and recommended chains per weapon, seal, sidearm and shell.

- [Variant port workflow](porting-variants.md): stable incoming sources, isolated alternate containers, per-variant colors, import repairs and identical texture sharing.

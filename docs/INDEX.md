# CSS research index

## Established findings

- CSS.Package v1 embeds metadata and author artwork in the pak. IoStore assets use isolated package namespaces. [Package guide](css-packages.md) documents the converter, verification and installation.
- All three `_P` packages are installed and discovered in-game. HIT2 gameplay was confirmed by the user. The paused-preview cloth defect is fixed by enabling the separate cloth tick on the disposable visual copy. [Cloth findings](preview-cloth.md).

- This game uses UE5.6, UE4SS d7e7826d Game/Shipping/Win64. Exact SDK DLL hash matches the installation. Native consumer code is C++23, clang-cl/MSVC ABI, release dynamic CRT.
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
| [animated-preview.md](animated-preview.md) | Paused-world animation, camera refresh behavior, live cleanup evidence and rejected additive idle |
| [README](../README.md) | User controls, independent state, installation and native reload workflow |
| [native-css-research.md](native-css-research.md) | Primary sources and exact native API research |
| [SDK-NOTES.md](../reference/ue4ss-sdk-d7e7826d/SDK-NOTES.md) | Exact local SDK, dependencies, hashes and build settings |
| [conversion manifest](../local-packs/conversion-manifest.json) | Ten relocated assets, source hashes and unchanged payload hashes |
| [prior investigation](../../investigation/2026-09-13/findings.md) | Attack-state recovery and live diagnostic evidence |

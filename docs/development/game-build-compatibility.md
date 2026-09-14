# Game-build compatibility

CSS keeps adapters for previously supported executables when adding a game hotfix. Package schema, package IDs, state schema and extension ABI are not changed to accommodate an executable address shift.

## Steam build 25265616

Installed and checked on 15 September 2026. Steam's local manifest reports build and target build `25265616`, depot manifest `8653917884651199139`, with download and staging complete.

Executable SHA-256:

```text
9e4a12dea9699ff6bcf0064273582ad06fc308d5387b6ae841eb1b75c3c057dc
```

The original color-mipmap adapter rejected this executable, and the live CSS log recorded that rejection. The old addresses now contain different instructions. Do not remove the guard or call the previous address.

| Adapter | Reflected CreateRenderTarget2D RVA | Call-site RVA | UpdateResourceImmediate RVA |
| --- | --- | --- | --- |
| Previously verified UE5.6.1 CL93241 executable | `0x3f28b70` | `0x3f28e5a` | `0x44c85d0` |
| Steam 25265616 | `0x3f28ba0` | `0x3f28e8a` | `0x44c8700` |

The new adapter was derived from the executable's direct call, PE unwind function boundaries and disassembly of the resource getter/render-command enqueue path. Runtime validation also requires the reflected `CreateRenderTarget2D` function to resolve to the matching wrapper. The call bytes and complete 32-byte callee prologue must match that adapter. Unknown builds fail closed. The previous adapter's addresses and byte checks are preserved exactly; the previous executable has not been rerun during this hotfix check.

The call uses `false` to preserve the drawn texture while updating its resource. The local UE5.6.1 source agrees with [Epic's UpdateResourceImmediate API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UTextureRenderTarget2D/UpdateResourceImmediate): the boolean controls clearing. Only engine code is enqueued on the render thread.

## Checks

The automated hotfix checks below passed. Evidence is retained locally under `work/hotfix-25265616/`; game binaries and personal state backups stay outside Git.

- New UE4SS session starts CSS. The removed MortalShell2Mod Lua entry is absent from the startup log.
- Nine installed CSS package trios pass embedded metadata, thumbnail, dye resource and container-hash validation. No outfit container needed rebuilding for this adapter fix.
- CSSX Unicode text input, draft preservation, 256-option search, empty search results and explicit selection pass the live development checks.
- Native Cheat Menu draft isolation, Apply, Discard and original damage-flag restoration pass with the old Lua mod absent. All 32 currently used controller function signatures resolve with the expected argument count. This is interface validation, not an inventory/progression mutation test.
- CSS opens with four native pages when CSSX is absent. Restoring CSSX returns five pages; Inventory, CSS, CSSX, Tarstones and Map all navigate successfully.
- Development and distribution Windows DLL targets compile. The nine portable test groups pass, including extension-failure cleanup and cleanup retry.
- All 28 installed outfit variants pass original-color restoration, an authored palette and matching Inventory preview material slots. The palette checks exercise 128 dye render targets. The complete original CSS state is restored and compared after the test.

These checks are not a claim that every game event, save, platform or unfinished Cheat Menu feature has been tested. The [native port checklist](cheat-menu-parity.md) remains the release gate for the Cheat Menu.

The previous executable's adapter and exact byte checks remain in the code. That executable was not rerun during this hotfix pass. Backward support here means retaining the previously verified path, not claiming fresh testing of every older Steam, GOG or subscription build. New Cheat Menu actions check their required reflected interfaces before mutation; an unsupported action reports the mismatch without replacing old adapters or changing outfit/state schemas.

## Future updates

Read Steam's installed build after staging finishes. Record the executable hash. Check the native adapter before invoking it, then check package discovery and reflected UI/gameplay interfaces. Re-run reversible in-game checks and restore the original CSS state afterwards. Keep old adapters and existing package/state formats unless there is a documented reason to migrate them.

The [official Steam announcements](https://steamcommunity.com/app/2584270/announcements/) retrieved during this investigation still led with the September 5 update. They did not supply a separate note for build 25265616, so this record uses the installed build and measured executable behavior rather than attributing unverified patch notes to the hotfix.

# Cloth overlays for beta.7

Status: integration design, not implemented or installed. The isolated branch
is `feature/cloth-overlays` at `work/cloth-core`. It starts from `0b243bb`,
including the separately committed translucent preview-light change. The main
worktree has another agent's socket-fit changes and must not be overwritten.

Unholy Genessa now separates opaque metal from translucent fabric on the same
six cloth surfaces. This avoids independent simulations drifting apart. Both
skin variants have the fabric materials saved as per-slot mesh overlays. The
mod's `authoring/layered-variant-check.json` and `work/layerpack1` contain the
source and cooked evidence. These assets still need CSS integration and live
acceptance before release.

## Required CSS behavior

- Allow a material control binding to select the base or overlay surface.
  Existing bindings must continue to select the base. Use a named selector,
  distinct from `association` and `layer`, which already address parameter
  layers inside a material. Reject unsupported selectors when loading a catalog.
- Create component-owned dynamic instances for overlay controls. Retain them
  through temporary game effects. Never change the shared skeletal mesh or
  material asset to store a player's selection.
- Apply palette colors, scalar values, texture choices and reset behavior to
  the selected surface. Update the wardrobe preview as well as the world mesh.
- Restore only component entries CSS still owns when changing outfits, replacing
  the pawn, closing the preview, unloading the core or recovering from an error.
  Preserve unrelated entries written by the game or another mod.
- Preserve native temporary overlay effects. The engine selects a non-null
  per-slot overlay ahead of the component's global overlay, so adding fabric
  without effect handling would suppress game effects on those sections.
  Establish the game's normal and active overlay states before implementing
  arbitration. A non-null pointer alone is not yet proven to mean an active effect.
- Keep any checks limited to worn outfits with authored overlays. Cache reflected
  fields and instances. Material updates belong on changes, not repeated loads,
  allocations or render-state rebuilds every frame.

This feature belongs to CSS. It must not add a CSSX dependency. It will be a
separate beta.7 commit, followed by a combined build that retains other beta.7
features. Do not install a DLL built only from this branch over the other
agent's newer work.

## Exact engine API boundary

The local 5.6.1 source is authoritative for this game:

- `SkinnedAssetCommon.cpp` serializes `FSkeletalMaterial::OverlayMaterialInterface`.
- `SkinnedMeshComponentHelper.h` reads asset defaults when component slot
  overrides are absent.
- `MeshComponent.h` exposes the reflected `MaterialSlotsOverlayMaterial` array.
  Its `SetOverlayMaterial` function takes one material and changes the global
  overlay. It is not a per-slot setter in 5.6.1.
- `SkeletalMeshSceneProxy.cpp` gives section overlays precedence over the global
  overlay in both draw paths.

The current online C++ documentation defaults to newer versions and shows a
different setter signature. Do not call it as though that signature exists in
the game. Epic's versioned [5.6 Python documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialEditorMeshComponent?application_version=5.6)
also describes the per-slot array and fallback behavior.

Direct array changes need a validated render-state refresh route. The stock
5.6.1 reflected setter does not provide that operation for an individual slot.
Resolve and test this boundary before adopting a runtime adapter. Do not assume
that changing an array's memory updates an existing scene proxy.

## Validation still needed

Catalog tests must cover legacy defaults, explicit base/overlay bindings and
invalid selectors. Runtime checks must cover Original, all palettes, custom
colors, reset, hidden sections, both skins, repeated variant switches, preview
recreation, pawn replacement and teardown. Check game overlay transitions and
restoration, including dash/travel effects, without overwriting foreign MIDs.

Build with the project's C++23 and pinned UE4SS SDK/import library. Measure
frame cost and inspect real DX12 gameplay. Linux authoring renders, shader
compilation and cooked-reference checks do not prove runtime correctness.

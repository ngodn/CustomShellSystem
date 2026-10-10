# Peony Astral material companions

Four Peony ghost parents and four opposite-culling instances now pass saved
asset verification in a fresh UE 5.6.1 process. They are private editor assets,
not installed or accepted in the game. Shader compilation, rendered appearance,
Windows cooking and runtime catalog registration remain.

## Ownership and behavior

`CSS_SeduXtress_eins0fx/work/peony-astral2` contains the candidate under
`/Game/CSS/eins0fx/Shared/Astral/Materials/`:

- `M_PeonyFabric`, `M_PeonyHardware`, `M_PeonyPanels`, `M_PeonyStockings`.
- One `MI_M_Peony*_SingleSided` variant for each two-sided parent.
- `T_NeutralMasks`, a shared placeholder with linear mask compression.

The parents copy Peony's source graphs and retain their scalar/vector controls,
base color, normals and trim-flow function. The existing native ghost graph
provides the form color and fade. Source color and emission feed the established
ghost-detail composition. Stocking opacity is multiplied by the ghost fade;
opaque surfaces use full coverage before that fade. Both fade and fixed-time
override default to zero.

Outfit texture defaults become shared placeholders. The authoring manifest
records each source parameter for the later runtime adapter; the ghost noise
uses the existing native game texture binding. Registry dependencies contain
only shared assets and Engine classes. Outfit textures remain owned by Eve,
and only the reusable trim function and ghost companions belong to the shared
pack. Existing Eve body/hair companions can be reused, subject to the final
cooked adapter audit.

## Saved sampler correction

The first candidate, `peony-astral1`, authored successfully but failed fresh
reload: assigning `T_NeutralLinear` changed mask samplers into linear-color
samplers. `texture-mismatch.json` records the exact differences. That candidate
is rejected.

The second candidate duplicates the neutral linear texture into the separate
`T_NeutralMasks`, sets `TC_MASKS` and disables sRGB. Mask samples now retain their
original sampler type after saving. Existing shared neutral textures are
unchanged. The verifier still requires exact texture paths and sampler types;
it was not weakened to accept the mismatch.

## Evidence and limits

Authoring and fresh reload both exit zero. The four parents preserve 37 scalar,
11 vector and 25 texture parameter entries in total, counting each parent
separately. Those counts include the ghost controls. All four opposite-culling
instances retain their correct parent and override. Skeletal, morph and cloth
usage flags are set. All protected source files retain their hashes.

`authored.json`, `reloaded.json`, commands and process receipts hold the checks;
`validation-receipt.json` pins 87 evidence, source and binary files. The helper
change permits canonical shared Astral material paths alongside legacy paths
and requires the game thread. It is an editor-only module.

Before integration, compile both required Windows shader targets, verify cooked
imports and material identities, merge the four bindings into the existing
runtime catalog, and test Faithful/Stray color, fade, opacity, palettes, flowing
trim and component visibility. Editor checks alone do not prove that supported
doubles reproduce Peony's customization correctly in the game.

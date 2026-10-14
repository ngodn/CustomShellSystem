# Native-human idle library, live bindings

All 60 choices exposed by Idle Animation resolve on original Genessa through
the installed CSS core and shared pack. Each selects its expected native clip
and BlendSpace in the player's actual animation instance. All 240 live
BlendSpace samples reference the expected sequence at rate 1.0.

This closes the full-library native idle availability/binding check that the
earlier Black Hole/Iris Out acceptance did not cover. It does not establish
visual quality, contact, full-clip playback, gameplay interruption or CINE takes.

## Inputs and execution

- Recipe: `tools/diagnostics/check_native_idle_library.py`, Python 3.14.7.
- Evidence: `work/three-outfit-rig-physics/native-idle-live-20261015/`.
- Installed core SHA-256:
  `d7cb0635d112f5fae73f67bc261c81d07df07eb0fef8797d786cc5d55191072f`.
- Native mesh: `/Game/Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/SK_Sester_Genessa_V6`.
- Native skeleton: `/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton`.
- Catalog: the combined candidate's `css.locomotion/manifest.json` under
  `build/eve-surface-package1/metadata/CSS_SharedAssets_P/`.

The game was running but unfocused, so no keyboard, mouse or gameplay inputs
were sent. The fixture temporarily wears original Genessa through the existing
CSS customization service. It checks that all 60 IDs are listed, then selects
each through the transient idle override. It reads the player's current
`ActiveBlendSpace`, `UseActiveBlendspace`, the selected shared sequence and
the actual carrier's `SampleData`.

For every choice, three samples read pelvis, head, hands and feet through
`GetSocketLocation`. Reflection signatures and bone existence are checked
before use. The 1,080 bone-position reads are all finite. These are sequential
reads, not simultaneous pose captures or a compression/retargeting comparison.
Pose choices can be nearly still: the smallest measured displacement is
`0.00000762939453125` cm. Movement magnitude is recorded, not treated as a
universal pass threshold. The first-to-last sample interval is 312.95 seconds.

The fixture checks the player, appearance, idle ownership, menus, held keys,
position and view between cases. The process exits zero. A separate receipt
inspection matches all 60 unique IDs to the manifest, all 240 live carrier
samples to the intended sequence, the executed recipe to the committed
diagnostic, and every finite bone sample to its expected structure.

## Restoration and inventory

Peony and her original customization/idle override are restored. Full saved
CSS state, the complete CINE model, player location/rotation, control rotation
and view target compare equal before and after. PID 1944353 remains the same.
An additional read after the fixture confirms the saved state and appearance.
No core, outfit or shared asset was installed or modified.

| Inventory | Count | Result |
| --- | ---: | --- |
| Existing shared-library entries | 63 | All already have native alternatives |
| Native idle choices | 60 | Listed, resolved and bound live on original Genessa |
| Native gait choices | 3 | Walk/jog/sprint not exercised by this stationary run; retain earlier conversion evidence |
| Live carrier samples | 240 | Correct sequence and rate |
| Bone-position reads | 1,080 | Finite; no visual/contact inference |

`inventory.json` retains a result for every catalog entry. `result.json` retains
per-choice handles, carrier samples and bone positions. The stage also retains
the executed recipe, before/after state and CINE snapshots, `verified.json`
and `SHA256SUMS`. Only small diagnostic records were created. Free space before
the run was 96G on root and 60G on theark; no project copy, cook or cleanup was
needed.

## Remaining checks

Retain the earlier native conversion, compression and offline surface evidence
in [the native-human record](../development/shared-animation-native-human.md).
This run checks one native mesh and short segments, including the beginning of
long clips. It does not close native CINE playback, target-specific facial
support, continuous loops/ends, contacts, action cancellation, other native
meshes or runtime performance. After Brute Battle's separate historical crash
is not claimed fixed by its short idle-binding sample.

The sidearm gameplay test remains waiting for game focus. The broader stable
release goal remains active.

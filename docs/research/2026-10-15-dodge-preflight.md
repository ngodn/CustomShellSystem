# Dodge preflight and frame-cap limits

The user authorized unattended tests in the open game. No dodge input or
frame-cap change was sent during this initial preflight. The subsequent
[twenty-case dodge matrix](2026-10-15-outfit-dodge-interruption.md) passes in a
separately checked uphill corridor. The notes below retain the initial limits.
The later [native frame-cap check](2026-10-15-native-frame-caps.md) also verifies
priority-preserving changes and restoration; the console-assignment limitation
below is not a remaining blocker for that native path.

Evidence: `work/three-outfit-rig-physics/dodge-interruption-20261015/`.
The current core remains `5f04a31900c8e323`, PID 1944353, with Peony selected
in Stray form. The full core hash and preservation checks are in the preceding
[attack-interruption record](2026-10-15-outfit-attack-interruption.md).

## Ground samples

Two attempts to call `LineTraceSingleForObjects` failed during argument
encoding. The bridge's `resize` flag is implemented for property writes only,
not function arguments. Both failures precede the native call. The original
recipes and logs are retained; do not repeat the same array workaround.

`ground-visibility.py` instead uses the live reflected `LineTraceSingle`
signature, an empty ignore list, self-ignore and no debug drawing. It samples
25 vertical visibility traces over XY offsets of -400, -200, 0, 200 and 400 cm,
from 50 cm above the pawn origin to 600 cm below it. Local UE 5.6.1 collision
profile source maps the first trace query to Visibility. This is a visibility
collision sample, not a walkability or hazard test.

Eight traces miss; three start penetrating. The center trace hits at a distance
of 155.0069 cm. Reflected inherited vector fields decode as empty objects, so
the record cannot establish impact normals. The fixture therefore does not
authorize an unobserved dash across this area. No dodge interruption pass is
claimed. The live ability inventory contains `GA_Dash_CorruptedGenessa`, rather
than an ability whose name includes Dodge.

`handoff.json` confirms unchanged player position, Peony, no idle override,
released input, the same process and `t.MaxFPS = 999`. The preflight writes no
game or CSS settings and changes no appearance, equipment or core.

## Frame-cap preparation

The existing [body-cost research](2026-10-15-body-cost.md) already records the
missing reflected `PlayerController.ConsoleCommand` function. A repeated
metadata lookup confirmed that limitation without invoking it. Reuse this
finding instead of trying that reflected path again.

The local UE 5.6.1 `UEngine::SetMaxFPS` implementation preserves the current
console priority except for constructor-to-scalability promotion. It is not a
reflected function. The current CSSX Performance and Perf Tuner setters use
console assignments and restore numeric values; they do not verify the original
priority. Restoring only 999 would not prove complete cap restoration.
[Epic's console-variable documentation](https://dev.epicgames.com/documentation/unreal-engine/console-variables-cplusplus-in-unreal-engine)
describes those priorities. No setter, native hook or memory write was attempted.

Continue with a verified priority-preserving cap path, or a restart-controlled
benchmark. The existing editor 30/60/120 Hz checks do not replace the live game
requirement. Root and theark free space are 96G and 49G respectively (`df -h`);
this work creates only small diagnostic records.

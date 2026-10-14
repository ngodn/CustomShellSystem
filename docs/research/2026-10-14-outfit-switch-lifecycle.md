# Repeated outfit and facial mesh switching

This check covers the nine original repair appearances plus Peony. It measures
owned components and direct mesh attachments around facial playback and across
repeated outfit changes. It is a stationary lifecycle check, not a movement,
rendering or frame-time benchmark.

**Result:** all 30 cases across three rounds pass independent receipt validation.
All 90 before/during/after snapshots retain the same 93 owned component handles
and 56 direct-child handles, with identical counts by class. Each case activates
its expected facial mesh and full-weight adapter, then returns to its original
mesh and animation-owner class. Full saved CSS state, the actual CINE model,
player transform and camera ownership/rotation restore exactly.

Evidence: `work/three-outfit-rig-physics/outfit-switch-lifecycle-20261014-2`.
The executed recipe, raw request log, per-case records, before/after snapshots,
installed TOC/size checks, independent `verify.py` and `verified.json`, final
live restoration readback, and checksums are retained. Game PID 1944353 and the
installed development core `54387cf5e27911f8` are unchanged. No new DLL or asset
was installed for this check.

The original fixture returned failure at its final comparison because it
compared the entire RPC envelope, including the unique request `id`. The saved
before/after replies differ only in that ID; their `result` models are exactly
equal. The raw failure record is preserved. Independent verification checks all
30 cases and every restoration invariant against the retained data, and a fresh
live readback confirms the restored appearance, saved state and CINE model.
The reusable diagnostic now compares the actual models. The 30-case run was not
repeated merely to replace that test-harness failure with a cleaner log.

## Method

`tools/diagnostics/check_outfit_switch_lifecycle.py` uses the existing CSS
customization/performance service and reflected component queries. The shared
manifest supplies expected normal/facial mesh mappings and animation adapters.
Installed outfit TOC hashes and bulk sizes are checked against that metadata.

For each of three rounds it wears Black Pearl, Prototype Planet Diving Suit,
Skin Suit, Vacation Bikini, Casual Knitwear, Midsummer Alice, Peony, both Unholy
Genessa variants, and Commander White. Each case:

1. Restores that appearance's existing customization and sets a temporary
   body-only idle so automatic facial playback does not compete.
2. Records the mesh, skeleton, authored/current post-process class, all player
   owned components and direct children of the player's mesh.
3. Starts Iris Out at two seconds, pauses after blending in, and requires the
   matching facial mesh, valid animation owner and enabled full-weight adapter.
4. Stops only the owned token and waits for the pre-performance mesh to return.
   It compares component counts by class before/after and against the same
   appearance's earlier round. Null or duplicate component handles fail.

Restoration preserves outfit-level and per-variant remembered customization,
the original appearance and idle override. It compares complete saved CSS JSON,
the CINE model, player transform, controller rotation and view target. No CINE
take, input, save-game edit, teleport or package replacement is requested.

Genessa Skin has the user's saved `css.unholygenessa.face.surprised` expression.
Its baseline is therefore already the facial mesh. Returning to that mesh is
correct; forcing a neutral mesh would discard the selected expression.

## Interrupted first run

`work/three-outfit-rig-physics/outfit-switch-lifecycle-20261014` completed all
ten first-round cases and four second-round cases. Counts stayed at 93 owned
components and 56 direct children. The next Casual Knitwear sample saw its
facial mesh change back while paused. The retained runtime status reports
`Player is moving` for token 26 at 2.612495996057987 seconds, and the player
position/rotation and camera angle changed. This is consistent with normal
gameplay interruption, not evidence of component accumulation.

Saved CSS state restored exactly. The fixture's whole-CINE-model comparison
failed and the player/camera geometry differed, so this attempt is not counted
as a complete stationary pass. The diagnostic did not move either back. The
user then explicitly left the character idle and authorized a new stationary
run. The updated fixture records before/after CINE models, reports a mid-sample
mesh change with its performance status, and checks for movement before starting
each next case.

## Remaining scope

This scene has fewer than 128 direct children. Even a passing repeated-switch
run cannot establish real-game recovery above 128, long-session/global-object
leak behavior, action/weapon sockets, cloth appearance, save/world transitions,
both forms/doubles or median/p95/p99 frame-time cost. The larger attachment
counts remain covered only by the existing host fixture until an appropriate
live case is measured.

Only small recipes, JSON snapshots and request logs are generated. No heavy
intermediates or new temporary installed files are needed, and the evidence is
retained for reproduction. Free space remains 91G on root and 66G on theark in
the before/after `df -h` readings for this check.

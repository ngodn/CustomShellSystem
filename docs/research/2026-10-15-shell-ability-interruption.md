# Genessa ability interruption

All twenty cases pass on the installed developer core `5f04a31900c8e323`,
PID 1944353. Iris Out and Come Closer release playback on the mapped Genessa
ability input across all ten release variants. The sequence has terminated;
independent archive verification and the final read-only restoration check pass.

Evidence: `work/three-outfit-rig-physics/shell-ability-interruption-20261015/`.
No production core or accepted outfit asset changed.

## Full sequence

`matrix/verified.json` verifies twenty distinct outfit/clip cases and forty raw
native ability-active observations. Both Stray clone actors appear in every case,
expire naturally, and leave zero active/cached CSS groups, no new fallback,
and no native input block. The stopped clip playheads range from 0.9503 to
1.4496 seconds. These are playback positions, not measured input latency.

The verifier compares the clone snapshots with the raw observer replies and
rejects seven altered records: active playback, natural clip completion, missing
native activation, an active leftover clone group, a missing secondary clone,
blocked input and changed Resolve. The clone controls preserve the equality of
the final snapshot and last sample, so rejection tests the intended condition.

The parent restores the original Peony appearance, idle and all saved outfit
memories. `restoration-verified.json`, produced by the retained
`verify-live-restoration.py`, independently reads the game afterward and confirms
the original position/view, saved CSS settings, CINE, health and Resolve,
inactive facial playback, released keys, no active clones or cached CSS groups,
and the same loaded core and player. Each child also verifies focus restoration.
`matrix/terminal-receipt.json` records the completed process and verification.

## Focused result

`peony-irisout-2/` records the mapped F press, two active native
`GA_AstralClones_Action` observations and an early facial-performance stop with
reason `A gameplay action is blocking input`. The stopped playhead is 1.0654
seconds, not a measured input latency. Both `AstralClonePrimary` and
`AstralCloneSecondary` are observed, then expire naturally. CSS groups return to
zero active/cached with no new fallback. The native ability and input blocks are
clear afterward. Observer snapshots match the raw probe replies.

The ability moves the player 458.39 cm in the checked uphill area. It uses the
existing corridor and restoration guards from the stationary dodge fixture;
this is a bounded test location, not a general terrain guarantee. Health and
Resolve remain unchanged. Original position/facing, camera, CSS settings, CINE,
released input and desktop focus restore. No resource or progression field is
written, and no clone is forcibly destroyed.

Seven deliberately corrupted copies are rejected: still-active playback, natural
clip end, missing native ability, missing secondary clone, leftover CSS group,
blocked input and mismatched resources. The earlier twenty dodge and twenty
combo archives still pass the extended verifier without rewriting their receipts.

## Retained diagnostic failure

`peony-irisout/` stops during read-only preflight. The clone observer belongs to
the established `css_probe` transport; requesting it through CSSX engine returns
`key target not found`. No input, camera, appearance or resource change was sent.
Saved state remains unchanged and facial playback is inactive. The corrected
fixture uses `css_live_snapshot.Probe` for observation, retaining engine calls
for the already established action/geometry checks. Object handles from these
two transports are not passed between them.

## Scope

This tests Stray's normal ability interruption and cleanup. It does not establish
Faithful ability behavior, clone attack/damage, full cloth/contact acceptance,
other forms' actions, or the rest of the stable-release requirements. The
Alteration restriction is preserved; no revival override is used.

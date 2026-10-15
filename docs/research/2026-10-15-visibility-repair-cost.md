# Visibility-repair cost comparison

Nine matched captures exercise the Trebuchaxe visibility-repair path on the
retained old core and the installed repair. All 162 injected owner-flag changes
are repaired. The 4,873 raw frames show similar MISC phase timings, without a
clear sustained regression. They do not establish an FPS improvement.

Evidence: `work/three-outfit-rig-physics/visibility-cost-20261015-retry2/`.
The capture and independent verification both exit zero. Final read-only
verification confirms original Peony, `eve_idle_702` playback, saved settings,
repaired mapped core, zero active/cached clones and released input.

## Method

The sequence is candidate, baseline, candidate, with three ten-second captures
per block. Each capture contains 18 calls that clear the hidden owner flag on
the same stowed Trebuchaxe, spaced 0.4 seconds apart. Every call is followed by
an observation that CSS restored the hidden flag. This deliberately exercises
`set_item_hidden`; a stationary capture without those events largely omits the
changed work.

| Build | SHA-256 |
| --- | --- |
| Installed repair | `5f04a31900c8e323a27f6ed5650649595015b6dea6600f6682db582dc8fcd619` |
| Retained predecessor | `4a4402a210f4d1e8c706726348237908be80472ef7df129b886ee657e48a5857` |

The established stationary reload workflow temporarily selects the native idle.
Reload guards require no active montage, facial performance, inventory operation,
clone or held input, plus unchanged player, equipment, resources and view. Both
reloads complete and map the requested core. The older implementation modifies
nested mesh flags; the original flags are restored after returning to the repair.
The final block verifies those flags stay unchanged throughout its captures.

The same scene, camera, Peony customization, equipment, native idle, focused
window and graphics settings are used throughout. The original cap remains
999; actual tick cadence is roughly 53.5 to 54.6 Hz. The fixture issues no cap,
save, resource, transform or cooked-asset writes. The temporary idle setting is restored
to the original choice afterward. No shot or movement input is sent.

## Measurements

Values are pooled per block, in milliseconds. MISC includes the entire cached
visibility pass and periodic enumeration, not exclusively the injected repair.

| Block | Frames | MISC median | MISC p95 | MISC p99 | MISC mean | MISC maximum |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Repair before | 1,643 | 0.06660 | 0.0949 | 0.1620 | 0.07033 | 0.5828 |
| Predecessor | 1,622 | 0.06835 | 0.0971 | 0.1691 | 0.07253 | 0.8322 |
| Repair after | 1,608 | 0.06795 | 0.0996 | 0.1674 | 0.07231 | 1.0616 |

The repaired core's second block has a larger single MISC maximum, while its
median and mean remain near the predecessor. Its per-capture MISC means are
0.07426, 0.07112 and 0.07155 ms, compared with the predecessor's 0.07207,
0.07303 and 0.07247 ms. This variability does not support a precise speedup claim.

| Block | Core median / p95 / p99 | Tick interval median / p95 / p99 |
| --- | --- | --- |
| Repair before | 0.1869 / 0.4280 / 0.6682 | 17.5884 / 22.5888 / 23.9030 |
| Predecessor | 0.1912 / 0.4610 / 0.8193 | 18.1059 / 22.8058 / 24.6082 |
| Repair after | 0.1931 / 0.4336 / 0.7000 | 18.4892 / 22.9286 / 23.9711 |

Tick cadence drifts across the sequence, including between the two identical
candidate blocks. These are elapsed developer-core phase measurements, which
can include thread preemption. They do not isolate cloth workers, GPU cost,
display presentation, or the cost of an individual visibility event. The result
covers this repair workload; broader outfit and lifecycle performance remains
separate.

## Verification and restoration

`tools/diagnostics/verify_visibility_repair_cost.py` rereads the raw captures,
checks their hashes, phase layout, sample validity and timing sums, recalculates
the distributions, and verifies the candidate/baseline/candidate sequence.
All 162 events fall after profile warmup and before capture completion. Their
count matches the raw engine-request log's actual Trebuchaxe owner setter calls.
Candidate component flags match the originals in every capture.

The parent verifies restoration of the selected core, complete saved state,
equipment flags, player/view, health/Resolve, graphics, CINE, input and desktop
focus. `final-live-restoration.json` additionally confirms active playback of
the original idle on the repaired mapped core. No test process remains running.

The initial attempt in `visibility-cost-20261015/` completed one candidate
capture but failed its flag guard because the native idle changed the game's
fist-weapon owner visibility. Those body weapons are explicitly excluded from
CSS's MISC ownership. The failed recipe and capture remain. Its independent
idle recovery passes without any core switch. The corrected fixture excludes
`Weapon.Slot.Body.*` from the equipment flags it owns and restores the idle even
if a later flag check fails. This does not relax checks on any CSS-managed item.

Reproduction uses `tools/diagnostics/compare_visibility_repair_cost.py --output`
with a new goal-workspace directory. It requires the same validated live setup
and stationary reload authorization; it is not a general core-reload safety test.

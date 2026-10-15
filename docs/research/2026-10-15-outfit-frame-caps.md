# Native frame-cap captures across release outfits

Stationary observation coverage is complete for all ten release variants: 90
captures and 40,763 raw timing records independently verify. This is not an
unqualified stability pass. Alice records two hair and two body resets in its
cap-60 condition, and the two earlier failed attempts remain in the combined
receipt. Full settings, appearance, view, graphics and core restoration pass.

Evidence: `work/three-outfit-rig-physics/frame-cap-20261015/outfits/`.
Parent log: `work/three-outfit-rig-physics/frame-cap-20261015/outfits.log`.
The installed core remains `5f04a31900c8e323`; no production code or assets changed.

## First run and reset investigation

`outfits/result.json` is terminal and failed. Both completed outfits have passing
`verified.json` receipts, with 4,063 Black Pearl and 4,133 Planet timing records.
Black Pearl's measured rates are about 29.94 to 29.95 Hz at cap 30 and 52.18 to
53.29 Hz at the higher caps. Selected caps are not actual achieved frame rates.

Skin Suit passes its cap-30 and cap-60 rig comparisons. At cap 999, both hair
and body reset counts increase by ten, so the matrix correctly stops. All nine
Skin Suit timing files remain intact and their statistics independently
recompute, including the slower final recording. That final recording contains
two supplied engine deltas above 66.6677 ms, with a maximum of 73.7944 ms.

The retained hair and body generators include resets for deltas above eight
120 Hz steps, as well as other reset conditions. This supports long-frame
recovery as a hypothesis, not a proven cause for all ten resets: the counters
were sampled only at boundaries, and the timed files omit the intervening
setup/readback periods. No solver thresholds or accepted assets were changed.
A retained V10 editor fixture independently shows the intended hitch path: an
injected 200 ms frame increments the reset count once, runs zero substeps, then
resumes two substeps on the next normal frame. Protected non-hair bones remain
unchanged. `historical-hitch-context.json` pins that file and the adjacent samples.
This is historical behavior, not a new replay, installed-asset identity proof or
an explanation of every live event.
`failure-analysis.json` retains source hashes, frame indices, both counter
changes and independently verified parent restoration.

`save-mtimes-readonly.json` records current save timestamps without opening or
changing save contents. The latest world save is at 07:43:28 local, later than
the final timed recording at 07:42:29 to 07:42:40. This does not establish saving
as the cause of those recorded spikes.

`skin-suit-reset-followup/` repeats only the original-cap condition with extra
reset-epoch, scale and owner-transform readbacks. Its three captures and 1,612
records pass independent verification. Hair/body reset counts remain at 2/4;
epochs, owner transform and scale remain unchanged. Measured rates are 53.15 to
53.98 Hz, with maximum supplied deltas of 26.55, 29.24 and 31.30 ms. Full parent
state/CINE/geometry/process restoration passes. This repeat does not erase the
earlier event or establish its cause.

`outfits-remaining/` is terminal and failed. Skin Suit passes nine captures and
4,123 records; Bikini passes nine captures and 4,125 records. Knitwear passes
cap 30, then records two additional resets on both hair and body at cap 60.
Its first cap-60 recording includes a supplied engine delta of 67.0674 ms,
above the authored long-frame threshold. Sampled reset epochs, scale and owner
transform remain unchanged. Boundary snapshots cannot explain every reset.
The failure, raw statistics and independent restoration check are retained in
`failure-analysis.json`, `terminal-receipt.json` and `restoration-verified.json`.

The diagnostic now offers explicit `--record-resets` collection. This records
increasing reset counters without stopping the remaining observations, while
still rejecting changed owners/limits, stalled counters and non-finite state.
A complete restored collection with resets exits 2 and remains `passed=false`.
The separate `--observations` verifier checks raw data, settings restoration and
recorded reset deltas; its receipt states `observation_integrity_verified`, not
unqualified stability acceptance. Strict verification still rejects resets.
Existing failures are retained, and no production solver threshold changed.

`outfits-observations/` is terminal, exit 2: all six remaining variants were
collected and restored, with Alice's reset observations retained. Combined with
`outfits/` and `outfits-remaining/`, its `combined-observations-verified.json`
checks exactly ten unique variants, all three caps, identical graphics context,
raw statistics and source/parent restoration. It reports
`observation_integrity_verified=true`, `passed=false` and
`all_attempts_passed=false`. Strict matrix verification rejects this collection.

`restoration-verified.json` independently reads the live game after completion:
original Peony and idle, saved state, CINE, inactive facial performance, position,
view, cap 999, console priority, graphics file and loaded core all match. No
production core or assets were changed. The final receipt records PID 1944353;
the capture process is finished. Root remains at 96G free and theark at 49G free
in rounded `df -h` output. No full project copy was created.

Eight corrupted reset-observation cases are rejected in
`observation-mode-controls.json`, including omitted events, incorrect deltas,
false pass status, decreasing counters, stalled updates, changed owners/limits
and non-finite state. The original Knitwear failure remains rejected in strict
mode. The existing Bikini archive still passes strict offline verification.

## Completed observations

Measured engine tick rates in Hz, range across three recordings per condition.

| Outfit | Cap 30 | Cap 60 | Original cap 999 | Additional resets |
| --- | --- | --- | --- | --- |
| Black Pearl | 29.94 to 29.95 | 52.18 to 52.50 | 52.48 to 53.29 | 0 |
| Planet | 29.91 to 29.94 | 53.16 to 53.46 | 53.29 to 55.20 | 0 |
| Skin Suit | 29.93 to 29.95 | 52.73 to 53.69 | 53.68 to 54.33 | 0 |
| Bikini | 29.93 to 29.96 | 53.17 to 53.52 | 53.81 to 53.98 | 0 |
| Knitwear | 29.93 to 29.95 | 52.42 to 53.73 | 51.88 to 54.86 | 0 |
| Alice | 29.93 to 29.94 | 40.66 to 56.28 | 54.75 to 56.42 | Hair +2, body +2 at 60 |
| Peony | 29.92 to 29.95 | 52.10 to 52.92 | 52.80 to 53.72 | 0 |
| Unholy Genessa, Eve | 29.91 to 29.94 | 49.91 to 50.11 | 49.90 to 50.81 | 0 |
| Unholy Genessa, original | 29.92 to 29.94 | 50.65 to 51.12 | 50.89 to 51.93 | 0 |
| Commander White | 29.94 to 29.95 | 53.27 to 54.46 | 53.18 to 53.95 | 0 |

Alice's cap-60 recordings contain two supplied engine deltas above the Eve
rigs' authored 66.6677 ms threshold, with a maximum of 74.1036 ms. The equal
reset/long-frame counts support the recovery hypothesis but are not synchronized
per-frame proof. Original Skin Suit and Knitwear failures remain separate.

Enabled primary rigs advance with finite sampled state and unchanged limits.
Unholy Genessa's hair rig is disabled in both sampled setups; only its active
body counter supplies update evidence. CW's active strand, body and Belt20 rigs
all advance without resets. The hand rigs lack these counters, and separate
cloth simulation is not measured.

Median CSS core tick cost ranges from 0.206 to 0.305 ms across these conditions.
That excludes animation-worker and GPU costs. This scene does not sustain 60 Hz
in the recorded higher-cap runs, and there is no matched baseline comparison.

## Measurement

This extends the [Peony cap captures](2026-10-15-native-frame-caps.md) to all ten
release variants. Each variant uses its existing customization and the common
`eve` idle, with three ten-second recordings at each native cap of 30, 60 and
the original 999. The frame profiler retains every raw interval, engine timing
and CSS core/phase timing. Actual measured cadence determines the rate tested;
selecting 60 does not itself prove 60 updates per second.

The established native setter preserves console priority. Each child checks the
saved graphics file, unrelated renderer values, CSS state, CINE, player/view
and original cap. The parent additionally pins the installed core and restores
the original outfit, idle and every outfit memory. No gameplay input is sent.

`sample_secondary_rigs.py` resolves the current mesh's post-process instance
and actual Control Rig nodes. It reads only fields present in live reflection:
enable values, counters, reset counts, step data, limits and available bounded
numeric state. Before/after comparisons require the same owners, finite
values, unchanged enable/limit/reset fields and advancing enabled counters.
Rigs without counters remain listed but do not acquire an update-timing claim.
Reads happen outside the timed recordings.

The read-only Peony preflight observes hair, body and hand rigs. Hair/body
counters advance and the original owners remain. Five deliberately corrupted
copies are rejected: stalled hair, stalled body, extra reset, changed owner and
non-finite state. The preflight and controls are stored one directory above the
matrix as `secondary-rig-preflight.json` and `secondary-rig-controls.json`.

## Reproduction

Uses the existing host Python 3.14.7 and installed developer core:

```sh
python tools/diagnostics/capture_outfit_capped_cost.py --output NEW_EVIDENCE_DIRECTORY
python tools/diagnostics/verify_outfit_capped_cost.py NEW_EVIDENCE_DIRECTORY
```

The capture refuses an existing output directory. It requires the recorded
native cap of 999 and existing outfit memories. Child recipes, commands, raw
recordings and restoration receipts are retained per variant. The independent
verifier checks complete variant coverage, recomputes timing statistics from
raw rows and runs the native-cap/counter negative controls. The earlier Peony
archive still passes the extended verifier with `--check-only`, without replacing
its historical receipt.

For diagnosis, `--only OUTFIT/VARIANT` and `--caps 999` select a focused case.
`--start-at OUTFIT/VARIANT` continues after separately verified earlier variants.
These selectors are recorded in the result. The full-matrix verifier still
requires complete coverage and all three caps, so a focused capture cannot
silently satisfy that gate.

The verifier can combine a completed continuation with an earlier restored
sequence using `--prior EARLIER_DIRECTORY`. It requires identical saved state,
CINE, player, core and initial geometry across sources and exactly ten unique
successful variant cases. Earlier failed cases remain in `prior_failed_attempts`,
with `all_attempts_passed=false`; `combined-verified.json` never overwrites the
failed capture's result. Coverage verification is separate from explaining an
earlier timing/reset anomaly.

## Limits

These are stationary whole-frame/core timing references and boundary rig reads.
They do not isolate animation-worker, cloth or GPU cost, compare an accepted
build against a candidate, or establish continuous visual/contact acceptance.
No simulation tuning follows from a selected cap or a single fast recording.
The separate matched-cost and movement/transition requirements remain open.

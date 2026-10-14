# Native frame-cap captures across release outfits

The first matrix stopped at Skin Suit's original-cap reset check and restored
all owned state. Black Pearl and Planet each pass nine independently verified
captures. A focused Skin Suit repeat passes three original-cap captures, but the
first reset event remains unresolved. A continuation from Skin Suit through the
remaining variants is running; complete matrix acceptance is still open.

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

`outfits-remaining/` is the current continuation, using `--start-at` Skin Suit.
Black Pearl and Planet are not repeated. Skin Suit is repeated at all three
caps because its first full result failed; remaining variants follow in the
same process. The diagnostic still rejects any additional reset. Each run
retains its own recipe and result; a partial sequence is not a ten-case pass.
The resumed Skin Suit case now passes all nine captures and 4,123 independently
verified records, with no additional resets. The other seven variants are still
being captured. Both Skin Suit repeats retain the original failure as a separate
finding, without changing the accepted solver.

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

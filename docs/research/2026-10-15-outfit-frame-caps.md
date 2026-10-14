# Native frame-cap captures across release outfits

The ten-outfit capture sequence is running. Do not treat the matrix as passed
until its terminal restoration and independent verification succeed.

Evidence: `work/three-outfit-rig-physics/frame-cap-20261015/outfits/`.
Parent log: `work/three-outfit-rig-physics/frame-cap-20261015/outfits.log`.
The installed core remains `5f04a31900c8e323`; no production code or assets changed.

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

## Limits

These are stationary whole-frame/core timing references and boundary rig reads.
They do not isolate animation-worker, cloth or GPU cost, compare an accepted
build against a candidate, or establish continuous visual/contact acceptance.
No simulation tuning follows from a selected cap or a single fast recording.
The separate matched-cost and movement/transition requirements remain open.

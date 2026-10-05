# Shared animation motion review

Checkpoint: 2026-10-06. Private offline evidence, not release acceptance.

The L2 batch now has 1,075 skinned replay images across its 13 moving clips.
They sample the unchanged 30 Hz UE snapshots every third frame. Maximum matrix
reconstruction error is 0.000931 cm, with zero measured angular error. These
renders use Black Pearl's V45C fitted authoring mesh and solid materials, before
game IK, secondary motion and live garment simulation. They do not validate all
six current Eve variants or replace the existing cooked-data comparisons.

`tools/authoring-probes/animations/summarize_motion_review.py` produces timestamped
sheets without skipping rendered samples. It checks the motion hash and records
the source rate, final covered time, omitted endpoint interval, contact extrema
and sheet hashes. Some stride-three previews omit the last one or two native
samples, at most 0.067 seconds; their summaries report that explicitly.

| Clip | Duration (s) | Images | Lowest/highest geometry relative to reference floor (cm) |
| --- | ---: | ---: | ---: |
| Bass Down Low | 12.667 | 127 | -5.110 / +8.913 |
| Black Hole | 20.000 | 201 | -3.679 / +14.749 |
| Eve Catwalk | 12.000 | 121 | -1.787 / +1.233 |
| Come Closer | 19.333 | 194 | -5.870 / +6.381 |
| Jang Won Young Spin | 3.667 | 37 | -4.344 / +5.199 |
| ATOOL Performance 1 | 4.000 | 41 | -3.706 / +1.652 |
| ATOOL Performance 2 | 6.667 | 67 | -1.447 / +2.856 |
| ATOOL Performance 3 | 6.667 | 67 | -1.025 / +3.841 |
| ATOOL Performance 4 | 2.000 | 21 | -15.136 / +42.922 |
| ATOOL Performance 5 | 4.333 | 44 | -7.388 / +148.619 |
| ATOOL Performance 6 | 6.667 | 67 | -1.025 / +3.370 |
| ATOOL Performance 7 | 6.667 | 67 | -1.629 / +0.713 |
| ATOOL Performance 8 | 2.000 | 21 | -0.573 / +159.530 |

The table measures rendered shoe/body geometry, not support-foot intent. A
positive value can be an intentional jump; a negative value still requires a
source and gameplay comparison before applying a correction.

## Inspected findings

All 201 Black Hole samples were inspected through eleven sheets, covering the
entire 20 seconds. The visible body and limb silhouettes remain coherent in this
view. This does not establish close hand contacts, every garment intersection,
temporal smoothness at native speed or final physics.

Its largest lift at 15.7 seconds matches a source hop: the source left/right toe
joints rise to 34.581/17.595 cm, compared with 3.968/3.968 cm at time zero. Both
source feet rise over 15.6-15.8 seconds. Do not flatten this interval to the floor.
The -3.679 cm target heel penetration at 4.2 seconds remains a contact check.
The source evidence records joint heights, not the source's skinned foot surface.

ATOOL Performances 5 and 8 move through wide overhead swings. Their first
renders crop parts of the body. Wider 4-metre camera rerenders were inspected
through all five sheets. The large lifts are associated with those whole-body
motions, not isolated foot spikes. Prop/support context, close deformation and
native-speed review remain open. Performance 4 and the initial intervals of
Performances 1, 2, 3 and 6 also received a first sheet review; do not count their
remaining sheets as inspected. Other clips have generated evidence, not completed
visual acceptance.

## Reproducible evidence

Under `work/cine-animation-port/`:

- `l2-blackhole-review-v1/`, `l2-blackhole-sheets-v1/`: all Black Hole samples,
  measurements, eleven reviewed sheets and input hashes.
- `l2-motion-review-v1/`: twelve other complete render jobs with commands and
  exit receipts. All returned zero.
- `l2-motion-sheets-v1/`: timestamped sheets and `batch-summary.json`.
- `atool-airborne-review-v1/`, `atool-airborne-sheets-v1/`: wider views for
  source-11 assets 004 and 007, Performances 5 and 8.
- `blackhole-source-review-v2/contact-source-check.json`: original source foot
  and toe heights around the hop, with the immutable source-track hash.

The source-joint review tool previously recorded only eight seconds and looped
shorter clips unconditionally. It now plays each complete source once, then holds
its final pose. Black Hole's fresh source video has 241 frames at 12 Hz and
20.083333 seconds including the final-frame hold, verified with ffprobe. Native
animation data is unchanged. Source hashes are checked after rendering.

No game files were changed for this review. The installed performance trial's
core selector, core hash, CINE entry and shared container hashes still match
`live-performance-1/deployment.json`; the game was closed at the final check.
The facial candidate remains uninstalled. Remaining work includes the other
sheet reviews, native-speed playback, all-outfit contacts/physics, live CSS/CINE
restoration and performance, two unresolved source rigs, and distributions.

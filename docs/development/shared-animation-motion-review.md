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
native-speed review remain open.

The remaining timestamped sheets have now been inspected. Every rendered sample
for all thirteen moving clips has a sheet review, with the wider views used for
Performances 5 and 8. This completes the sampled silhouette pass, not native-speed
or gameplay acceptance.

| Clip | Observed motion and follow-up |
| --- | --- |
| Bass Down Low | Standing dance, arm gestures and a turn into a rear-facing pose with hands behind the lower back. The end differs substantially from the start. Keep one-shot playback; inspect hand/back contact closely. |
| Eve Catwalk | Cross-steps, hand-on-hip presentation and front/back pivots, with repeated phrases. Looks coherent at sampled intervals. Endpoint velocity still needs a loop transition; this is not ordinary walk locomotion. |
| Come Closer | Standing dance with arm gestures, pivots and heel lifts. The final interval settles into a different pose. The lowest heel position is -5.870 cm at 16.5 seconds; retain this as a contact follow-up. |
| Jang Won Young Spin | Turn with lifted/crossed leg and a rear-facing finish. No gross limb discontinuity visible in the sampled view. Keep one-shot playback. |
| Performance 1 | Arms held overhead with small body sway. Support/prop context remains unverified. |
| Performances 2 and 3 | Different opening gestures lead into a raised-arm presentation, followed by lowering the arm and returning to standing. Similar middle/end motion is not evidence that the complete clips are duplicates. |
| Performance 4 | Airborne bent-leg gesture. Last sample returns near the reference floor, while the opening has a -15.136 cm intersection. Source/contact comparison remains required. |
| Performances 5 and 8 | Whole-body overhead swings, reviewed with the wider framing described above. Preserve the lifts; support and prop context remain open. |
| Performances 6 and 7 | Opening bow/hand gestures, pivot into a held presentation, then return to standing. Similar held sections have different entry motion. Raw ponytail poses extend sideways during the turn; live secondary motion remains unverified. |

No gross detached or stretched limb was visible in these sampled views. Small
hand intersections, footwear contact and hair/garment behavior still require the
closer source comparisons and game checks. The images have no visible floor;
their captions measure geometry against the reference plane.

## Endpoint audit

`audit_motion_seam.py` reads the unchanged raw snapshots, compares first/last local
transforms and measures one-sided translational velocities at both ends. It
retains a per-bone report and source hash. It rejects invalid snapshots, mismatched
bone order, zero quaternions and nonfinite transforms. Quaternion sign changes
are treated as the same orientation. This is a diagnostic, not an automatic
loop classifier, and does not measure angular velocity or world-space contact.

The largest orientation gap among the twenty recorded principal body bones is
157.522 degrees for Bass Down Low, 102.182 for Black Hole, 100.213 for Come Closer
and 109.237 for Jang Won Young Spin. Those raw endpoints are not seamless loops.
Catwalk's corresponding orientation gap is only 0.086 degrees, but its largest
local translation velocity gap is 49.913 cm/s. A matching endpoint pose alone
therefore cannot establish a smooth Catwalk loop. The audit changes no playback
mode or source data. All thirteen moving clips have reports.

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
- `l2-seam-audit-v1/`: per-bone endpoint reports for all thirteen moving clips
  and their summary. An initial broad input selection also audited photo000,
  then correctly rejected photo001's short static timeline. The completed batch
  explicitly selects moving clips; static photo poses are not loop candidates.

The source-joint review tool previously recorded only eight seconds and looped
shorter clips unconditionally. It now plays each complete source once, then holds
its final pose. Black Hole's fresh source video has 241 frames at 12 Hz and
20.083333 seconds including the final-frame hold, verified with ffprobe. Native
animation data is unchanged. Source hashes are checked after rendering.

No game files were changed for this review. The installed performance trial's
core selector, core hash, CINE entry and shared container hashes still match
`live-performance-1/deployment.json`; the game was closed at the final check.
The facial candidate remains uninstalled. Remaining work includes
native-speed playback, all-outfit contacts/physics, live CSS/CINE
restoration and performance, two unresolved source rigs, and distributions.

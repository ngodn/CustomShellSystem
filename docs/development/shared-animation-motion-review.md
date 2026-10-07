# Shared animation motion review

## Final-layer contact check, October 7

The four standing dances with substantial neutral penetration now have a
60 Hz capture through Black Pearl's actual performance adapter. All 3,344
evaluated frames retain the existing body/finger and restoration checks.
The diagnostic records the four component-space calf/foot transforms that
influence every vertex in the audited footwear material (25,152 referenced
vertices). It then skins that surface independently from the captured pose.

| Clip | Frames | Lowest sole Z (cm) | Time (s) |
| --- | ---: | ---: | ---: |
| Bass Down Low | 761 | -5.266 | 4.333 |
| Black Hole | 1,201 | -3.885 | 4.217 |
| Come Closer | 1,161 | -6.301 | 16.533 |
| Jang Won Young Spin | 221 | -4.344 | 2.600 |

The outfit's downstream post-process layer does not remove this penetration
in the identity preview scene. At matching source-key times its surface differs
from the previous independent neutral decode by at most 0.000365 cm in the first
three dances and 0.004797 cm in the separately recooked Spin. Black Hole's new
minimum occurs between the original 30 Hz samples. This does not establish the
world floor or include the appearance's component offset, body customization,
terrain or cloth simulation.

Original source foot/toe samples at these times remain above the source reference
plane. Spin has one lifted leg; the other three samples have both feet low.
These are joint measurements, not source sole geometry. Preserve those support
and lifted-leg distinctions when preparing a correction. The result narrows the
next step to contact correction and a world comparison; repeating raw-versus-
post-process captures for these same four candidates is unnecessary.

Evidence: `work/cine-animation-port/shared-evaluated-contact-v1/` contains the
four captures, build and editor receipts, `surfaces.json`,
`neutral-comparison.json` and `source-support-samples.json`.
`measure_evaluated_contact.py` checks the source mesh, skin hashes, transform
space, full frame count and finite transforms. Four malformed-input checks
reject wrong space, missing frames, another mesh and a nonfinite pose without
writing an output. The helper builds with UE 5.6.1. Capture is opt-in through
`CSS_PERFORMANCE_CAPTURE_FEET=1` and affects authoring diagnostics only.
No animation, runtime DLL, installed package or saved player setting changed.

## Full-frame continuation, October 7

All eighteen original moving clips now have complete videos at their captured
native rates, including every endpoint: 5,830 frames, 161.85 seconds including
the final-frame holds, and 21,657,044 encoded bytes. The batch uses
`run_full_motion_renders.py`, two renderer workers, streamed MP4 output and
independent ffprobe frame-count/rate/duration checks. Rendering completion is
not a claim that every second has been visually accepted.

Evidence: `original-full-render-v1` and the separate `spin-full-render-v2` retry
under `work/cine-animation-port/`. The original batch correctly reports failure
because Spin stopped at frame 73, at 0.001012 cm reconstruction error against a
0.001 cm diagnostic bound. Its complete measurement and rerender use the same
explicit 0.003 cm bound already used for the scene clip. Spin's measured maximum
is 0.001018 cm with zero angular difference. The original failed job remains.
No source key or cooked-motion comparison bound changed. The maximum across
all completed renders is 0.001369 cm.

The all-key temporal scan exposed a Spin neck rotation that the earlier sparse
silhouette review missed. At frame 66 the source and cooked lower-neck world
rotations change by 170.955 and 170.957 degrees. The head remains continuous,
with a maximum 14.492-degree step. The source neck roll and compensating head
rotation are preserved numerically, but a close render of frames 64 through 67
shows a pinched collar before the roll. Source fidelity alone does not make
this deformation acceptable on the target's two-neck-bone chain.

An offline correction in `spin-neck-correction-v1` redistributes neck twist
between the chest and head while retaining the existing joint directions. It
changes only the three local rotations `neck_01`, `neck_02` and `head` across
111 frames. All local translations/scales are unchanged. The head and the rest
of the body preserve their world transforms to numerical precision, apart from
the neck and its six-bone tie branch, which follows the corrected attachment.
Maximum preserved position error is 1.51e-13 cm. Neck world-rotation peaks fall
to 34.163 and 23.748 degrees. The four-frame close comparison removes the visible
pinch. Its complete 111-frame, 30 FPS replay also passes reconstruction and
encoded-clock checks (`full-render-verification.json`). The prototype was
initially uncooked. The follow-up below records its saved and cooked verification;
full visual and gameplay acceptance remain separate.

The source comparison also checks After Brute Battle's largest local calf
rotation. The source contains a fast turn at the same frame, but retargeting
changes the joint distribution. The target's largest calf world step is 45.981
degrees, compared with 52.512 in the source. This does not establish contact or
skin quality. Evidence: `shared-contact-v1/source-temporal-comparison.json`.

`shared-contact-v1/contact.json` measures all 6,945 original cooked key poses on
the current Black Pearl footwear section with mesh translation retargeting.
Neutral floor penetrations remain, including Bass Down Low (-5.266 cm), Black
Hole (-3.739 cm), Come Closer (-6.301 cm) and Spin (-4.344 cm). These measurements
exclude final post-process simulation and component placement. Black Pearl's
released ground offset is -3 cm; the other five Eve variants use zero. Compare
the complete gameplay pose and source support intent before changing contact.
Do not flatten scene jumps or apply one outfit's component offset to every rig.

The source and target use different neck-chain lengths. Epic documents chain
mapping and retarget-pose alignment for that case; the specific twist defect
above is established by our captures and close renders, not asserted by the
documentation. [UE 5.6 IK retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.6).

### Saved and cooked Spin correction

`derive_neck_twist_correction.py` reproduces the prototype exactly, refuses an
existing output directory and rejects scaled neck chains before writing output.
`prepare_neck_twist_clip.py` creates only
`/Game/CSS/Locomotion/Studies/Neck1/AN_JangWonYoungSpin`, preserving the original
asset. It edits the three reviewed rotation tracks and reuses the owned
`Studies/Precision1/ACS_Motion` compression setting.

Creation and fresh editor readback compare all 111 frames. Translations, scales
and other local rotations match exactly. Maximum corrected quaternion distance
from the prototype is 2.843e-7. Both editor captures match each other exactly.
The Windows cook and independent decoder pass; full cooked-key comparison has
maximum translation error 0.000835 cm, quaternion distance 0.000173 and scale
distance 9.611e-7, below the unchanged shared-library bounds.

The cooked world-pose comparison checks 371 joints outside the corrected neck
and attached tie branch. Maximum position difference from the original motion
is 0.000874 cm; maximum orientation difference is 0.022862 degrees. The head's
position difference is 0.000393 cm. The nine requested outfit meshes pass all
81 existing adapter cases, including body/finger comparison and restoration.
This does not validate simulated cloth or the user's gameplay state.

Evidence is in `spin-neck-ue-v1`, including `derivation-checks.json`,
`prepare-result.json`, `verify-result.json`, `comparisons/00.json`,
`cooked-world-preservation.json` and `adapter-result.json`.
`shared-neck-trial-v1` replaces only this animation in the combined pack,
preserves the other 98 exports, and passes both host loader configurations.
The original source, prior pack and failed-render evidence remain intact.
The new pack is not installed.

## Cooked standing-dance contact corrections

The four measured standing dances now use isolated `Studies/Contact3` copies.
Their durations and 30 Hz authored clocks are unchanged. The solver changes
only the six thigh/calf/foot tracks. Its exact authored-data checks preserve
every other track, including Spin's reviewed neck correction. Source assets and
released outfit packages remain untouched.

The first bake exposed an additional problem: nearly straight source legs have
unstable geometric knee directions. Preserving those directions while bending
the leg for clearance amplified between-key dips. `plan_dance_knee_support.py`
uses neighboring bent poses as thigh-local direction anchors only below
15 degrees of original knee flexion and only when clearance needs a lift.
Its influence eases with flexion and lift depth. It rejects missing or conflicting
anchors rather than guessing. The largest measured anchor separation is
19.383 degrees, below the 45-degree rejection limit. Four focused tests cover
airborne/bent preservation, straight-leg reversals, edge spans and rejection.

The final pass uses measured 60 Hz post-process residuals, covering each key's
adjacent half-frame extrema with a two-key taper. Maximum extra lift is
0.882 cm. This changes the clearance envelope, not playback speed or sample rate.
Fresh editor verification repeats all four bakes and compares every saved key
exactly. Windows cooking and independent decoding pass the original comparison
bounds: at most 0.000955 cm translation, 0.000220 quaternion distance and
9.611e-7 scale distance.

| Clip | Final decoded samples at 120 Hz | Lowest neutral footwear Z (cm) |
| --- | ---: | ---: |
| Bass Down Low | 1,521 | -0.038117 |
| Black Hole | 2,401 | -0.002295 |
| Come Closer | 2,321 | -0.041136 |
| Jang Won Young Spin | 441 | -0.091558 |

The measurement uses all influences of the actual Black Pearl footwear section
and its mesh translation-retargeting rules. None of these 6,684 samples exceeds
the existing 0.1 cm penetration reporting threshold. This does not establish
continuous contact between samples or world placement on other outfits.

All four clips also pass the existing 60 Hz compressed animation/post-process
checks on all nine requested outfit meshes: 36 clip/mesh pairs and 324
body/finger/restoration cases. These checks do not simulate world cloth.
Selected close renders show coherent leg shapes at the reviewed former failure
times. Spin's traveling poses needed a camera that follows horizontal pelvis
motion; `render_skin_contact.py --follow-pelvis` changes only that camera.
The selected still review is not a full temporal or live visual pass.

Evidence: `dance-contact-v3/{prepare-result,verify-result,cooked-dense-contact,
all-outfits}.json`, `comparisons/00.json` through `03.json`, and the per-clip
review directories. The earlier two iterations and source measurements remain.
`shared-contact-trial-v1` combines the four replacements while preserving the
other 95 exports byte-for-byte. It still has 63 entries and 99 exports, passes
both host loader configurations and installation preflight, and is not installed.

Epic's [Two Bone IK documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-two-bone-ik-in-unreal-engine?application_version=5.6)
describes the separate end-effector and joint-target roles. The need for this
particular pole stabilization and clearance pass comes from our captured poses
and measured surfaces, not from a general recommendation to modify every clip.

## Earlier sampled review

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
The facial candidate was still uninstalled at that review check. It has since
been installed as the combined private trial; see
[the expression installation record](eve-expression-candidates.md#combined-private-trial-installation-2026-10-06).
Remaining work includes
native-speed playback, all-outfit contacts/physics, live CSS/CINE
restoration and performance, two unresolved source rigs, and distributions.

## Remaining five moving clips, 2026-10-06

All 555 rendered samples from the other five moving candidates were inspected
through thirty timestamped sheets. The camera follows pelvis translation without
altering the motion. This completes the sampled silhouette review of the eighteen
currently retargeted moving clips, not native-speed or gameplay acceptance.

| Clip | Samples | Observations |
| --- | --- | --- |
| Adam Catwalk | 152 | Standing weight shifts, short steps and pivots, then a return toward the opening stance. Limbs remain coherent in these views. Lowest measured shoe contact is -1.965 cm. |
| Lily Catwalk | 67 | Raised-hand gestures, turns and a small hop around 2.0-2.2 seconds, then a return toward the opening gesture. Measured contact spans -2.645 to +11.478 cm. |
| ATOOL Gesture 200 | 10 | A short right-arm lift over 0.36 seconds. All source samples were rendered. It is not a complete repeating idle. |
| ATOOL Performance 801 | 189 | Opening gestures, a leg lift and turn, followed by a held pose after about 3.3 seconds. The hold matches the previously inspected source action, whose varying channels end at frame 200. Preserve the full 18.85-second source duration. |
| After Brute Battle | 137 | Falls, crouches, jumps and rotations, ending curled forward. This scene body track needs its source staging: it travels roughly ten metres, and the rendered reference-plane range is -30.992 to +474.923 cm. Do not classify those scene heights as ordinary walking contact or flatten the airborne motion. |

The first After Brute Battle render stopped at frame 216 with a reconstruction
difference of 0.001051 cm, above the default 0.001 cm bound. Blender returned zero
despite the Python assertion; the absent report exposed the incomplete run.
The replacement command uses `--python-exit-code 1` and requires the report as
well as a successful process exit. Its explicit 0.003 cm diagnostic tolerance
does not modify motion data. The completed sampled run measured a maximum
0.001221 cm position difference and zero angular difference. The earlier
all-frame measurement in `l4c-measure-v2` remains the stricter complete numeric
record; this render supplies the missing visual coverage.

Evidence under `work/cine-animation-port/`:

- `remaining-motion-review-v1/`: Adam, Lily, 200 and 801 renders and reports,
  plus the retained incomplete After Brute Battle attempt.
- `remaining-motion-sheets-v1/`: inspected sheets and summaries for those four.
- `remaining-camerabody-review-v2/`, `remaining-camerabody-sheets-v2/`: completed
  After Brute Battle review, with the command/exit receipt alongside.

These diagnostic views exclude gameplay IK, cloth and secondary motion. Native
speed, closer hand/garment checks and source-scene contact remain outstanding.
No installed game files changed during this review.

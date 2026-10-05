# Shared animation library investigation

Updated 2026-10-05. Active scope: [CSS and CINE animation port goal](../../extensions/cine/docs/ANIMATION-PORT-GOAL.md).
Protocol Echelon Proxima remains paused.

## Source inventory

`work/cine-animation-port/inventory.json` records 13 original archive hashes,
95 member files, 106,949,081 compressed bytes and 264,507,413 unpacked bytes.
`extraction.json` records isolated extraction and per-file hashes. Original
archives were hashed before and after extraction. No bundled program was run.
Each animation container is mounted separately with the installed source game.

Metadata inspection of the dance, catwalk and animation-test archives found
19 AnimSequence assets and two DataTables. Of those sequences, two are static
Eve placeholders used while companion catwalks play. The actual Adam and Lily
catwalks have their own skeletons and remain in scope.

| Source | Animation paths below `/Game/Mods/HD-ATOOL_P/anim/` | Source rate | Decode status |
| --- | --- | --- | --- |
| Bass Down Low | `513` | 30 Hz | Full source keys decoded |
| Black Hole | `543` | 30 Hz | Full source keys decoded |
| Catwalk | `526`, `527a/1`, `528a/1` | 30 Hz | Eve, Adam and Lily keys decoded |
| Catwalk support | `527`, `528` | 30 Hz | Static Eve placeholders, no varying tracks |
| Come Closer | `514` | 30 Hz | Full source keys decoded |
| DumbDumb | `971` | 60 Hz | Seven unresolved animated bone indices |
| Iris Out | `970` | 60 Hz | Variable-key audit decoded; seven unresolved indices, two animated |
| Jang Won Young Spin | `985` | 30 Hz | Full source keys decoded |
| ATOOL animation test | `1` through `8` | 30 Hz | Full source keys decoded |

The Jiggle patch contains Lua support files, not a character clip. Full package
readback now covers all 85 paths in ATOOL main, k2 and k3, including their
Blueprint exports. They contain two identical static support sequences at
`data/1` and `data/2`, not additional body performances. Main versus k2 differs
only in ModActor; k3 differs in nine Blueprint packages. See
`work/cine-animation-port/atool-comparison.json`. This table is not a release
acceptance list.

The authoring examples were inspected in Blender 5.2.2 with auto-execution
disabled and unchanged source hashes. `200.blend` has a short body/facial
example on a 25 fps scene timeline. Its two supplied `200.fbx` files are
byte-identical. `801.blend` contains a 1131-frame-range body action on a 60 fps
scene timeline, helper actions and two rigs. Raw action ranges alone are not
accepted playback boundaries. Full motion and constraints still need review.

ATOOL also references 31 base-game photo poses and 21 facial expressions.
The user includes the photo poses and, after correcting an earlier answer,
requests facial support where feasible. All 52 referenced packages load as
AnimSequence assets. The photo poses contain 2 or 7 samples at 30 Hz. Do not
confuse a pose such as `Somersault` with a full acrobatic performance based on
its name. Inspect its actual samples and select appropriate hold behavior.
All 31 photo poses also pass strict bone-track decode. The facial sequences contain named float curves; their exact timing and mapping
must be preserved. Reference lists and full metadata are retained under the
same work directory.

## Decoder evidence

Tools use .NET SDK 10.0.401 and the pinned CUE4Parse revision
`bbb3551af4c0c1bf481bc2821a18bd3b72706ff4`. Mapping SHA-256:
`713b1c83b6cd05f63d7cfa38e0e2d2c87587bca0785acd7167c75d44c0205bd7`.

The existing source exporter accepted only per-track compression. Iris Out uses
`AKF_VariableKeyLerp`, which the pinned CUE4Parse decoder already supports in
`Legacy/AnimConverter.cs`, `ReadKeyLerpData`. Both source export and the diagnostic
audit now dispatch this format without changing timeline or skeleton validation.
The source-01 per-track JSON is byte-identical before and after the change,
SHA-256 `1ba485617cb579ee7aadf21a207147ab3a573ff58264a679b4f63a61f57a5c0c`.

The candidate exporter is isolated in `work/cine-animation-port/exporter-v1`.
Its first isolated launch lacked the CUE4Parse project dependencies; the missing
files were copied from the established exporter with hashes recorded in
`dependency-copy.json`. `tracks-v1` preserves that failed launch and
`tracks-v1deps` preserves the subsequent results. The installed exporter is
unchanged.

Both `970` and `971` use out-of-range indices 3269 through 3275. Their resolved
retail skeleton has 3267 bones. The authoring archive's skeleton was also decoded
and has 3267 bones, so it does not resolve the missing entries. The audit retains
all 158 tracks, including unnamed tracks. Strict source export still rejects
these clips. Do not label them retargetable or discard the unknown tracks.

`AssetReadback` now accepts the same explicit Stellar Blade source-game override
as MeshExport. Its default remains UE 5.6. Full-package inspection resolved the
12 generated-Blueprint packages that the leaf-name object lookup could not
find in each ATOOL variant. The unchanged MS2 `A_Shared_Idle_H2` fixture produces
byte-identical JSON before and after the change, recorded in
`readback-regression.json`. The isolated build succeeds with one NuGet
vulnerability-cache warning, not a source/compiler error.

## 60 Hz preparation

The initial 60 Hz requirement was corrected after the user clarified it was a
question. Preserve source rates by default; use resampling only where beneficial.
Python 3.14.7 preparation now accepts an
explicit sample rate; native 60 Hz retains its frame indices, and 30 Hz becomes
60 Hz without changing endpoint duration. Fractional source-frame times go
through the original channel time arrays. Translation and scale use linear
interpolation; rotation uses normalized shortest-arc quaternion interpolation.
This matches the local UE 5.6.1 per-track decoder at
`AnimEncoding_PerTrackCompression.cpp:528-538`.

The UE import and pose-review script no longer assumes 30 Hz. It reads each
prepared clip's rate for import and sampling. The existing C++ retarget helper
already copies the source data model's rate and uses source frame times.
Epic's [5.6 AnimDataController documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AnimDataController?application_version=5.6)
describes setting the sampling rate and recalculating frame count from play length.
Set rate before supplying the intended frame count and keys, then verify editor
and cooked readback. Those editor/cook checks remain pending for this library.

Evidence in `work/cine-animation-port/sample60-verification.json`:

- Bass Down Low: 381 source samples become 761 samples at 60 Hz, duration
  12.666667 seconds. All 54 selected body/finger tracks preserve every original
  sample exactly at even output indices. The immutable decode retains all other
  source tracks for further review.
- Existing 30 Hz preparation is byte-identical to the pre-change script for the
  same clip, configuration and inputs.
- Ten timing/interpolation tests pass, covering native 60 Hz sample preservation,
  30-to-60 timing, irregular key times, opposite quaternion signs, missing and
  constant channels, legacy 30 Hz output, and invalid timelines/downsampling.

These checks establish source preparation only. They do not establish good
contacts, transitions, clothing deformation, loop seams or game performance.

## First retarget and face compatibility check

`retarget-l1` is the complete 381-sample Bass Down Low candidate at its native
30 Hz, using the established 90-degree source basis and 54 body/finger bones.
It imports and retargets in UE 5.6.1. A fresh process compares all 381 samples
across 388 evaluated target bones, both saved and repeated conversion, with
zero positional/quaternion difference. Existing Black Pearl mesh and shared
skeleton hashes remain unchanged. This is a private AnimLab candidate.

The skinned preview replays every third source sample on the fitted Eve mesh.
Its 127 rendered frames are a 10 fps diagnostic, not the asset's playback rate.
Replay positional error remains below 0.000445 cm. Lowest contact geometry
ranges from -5.110 to +8.913 cm relative to the reference floor. Inspect support
feet and source root motion before grounding; do not flatten intentional lifts.
This candidate is not accepted for release yet.

The L2 batch now contains 13 decoded Eve performances and all 31 photo poses,
preserving their native sample rates. UE 5.6.1 imports and retargets all 44.
Fresh-process verification checks 3,410 frames for both saved playback and
repeated conversion (88 comparisons), with zero position/quaternion difference
and unchanged protected mesh/skeleton hashes. Receipts are
`work/cine-animation-port/retarget-l2/retarget-result.json` and
`readback-result.json`. These are offline candidates, still awaiting contact,
deformation, loop/hold and live review.

The remaining `CameraBone.blend` support example is now inspected with embedded
scripts disabled and its source hash unchanged. It contains a two-bone camera
rig and a 201-bone Eve rig. `c2` animates the camera over frames 0-410; the Eve
action `MV_SD20_AfterBruteBattle_QTE_Main_Eve_A` spans 0-820 and has 617 varying
curves. The scene is 30 fps. Do not classify the file as camera-only or assume
that the full Eve action uses the scene's current playback timing without
checking its source. Both actions remain inventoried, not ported or accepted.
Receipt: `work/cine-animation-port/blender-inspection/camera-bone.json`.

The Adam and Lily catwalks now have separate offline candidates (`retarget-l3a`
and `retarget-l3l`). Their rest poses are not replaced by Eve's. Adam's extra
`Base` ancestor is retained, including its source keys, giving 55 source bones;
Lily uses 54. The preparation tool requires explicit ancestor inclusion rather
than flattening an omitted parent. It rejects missing joints, duplicate names
and invalid/cyclic parent ordering. Four new hierarchy checks and the ten
timing/interpolation checks pass. Repreparing the original Eve fixture produces
byte-identical source mesh, sampled clip and batch configuration.

Fresh UE processes verify all 454 Adam frames and 200 Lily frames, with zero
saved/repeated position or quaternion difference and unchanged protected
target assets. These checks validate repeatability, not the artistic result.
Full-motion skinned renders are the next review step. Their source archives
and the previously released outfits remain unchanged.

The first 31-pose skinned gallery is rendered and reviewed at the final source
sample of each photo pose. Most silhouettes are coherent, but this single-view
gallery does not establish hand contacts or garment acceptance. The fixed
camera crops the airborne Somersault and WeDidIt poses. Measured contact height
also varies among grounded poses (for example Natarajasana is +10.95 cm), while
the airborne poses reach +99.20 and +72.36 cm. Do not flatten every pose to the
floor. Compare source support, footwear and root motion before correction.
Evidence: `photo-pose-gallery-labels.json`, `photo-pose-render/report.json` and
`photo-pose-contact-sheet.jpg` in the private work directory.

The review renderer now accepts explicit camera height and orthographic scale.
Wider rerenders of both airborne poses show the complete body and preserve
their original root height (`photo-airborne-render`). The raised thigh in
WeDidIt shows a stocking/body intersection in this raw-pose preview. Keep it
open for garment and final-component review; successful pose comparison does
not resolve it. The renderer explicitly disables embedded Blender scripts.

Facial source metadata uses 99 distinct curve names. The retained Eve Body in
`CSS_SeduXtress_HeelSupportsV45C.blend` has 97 exact shape-name matches. The other
two are capitalization differences (`Jaw_Left`/`jaw_left` and
`Jaw_Right`/`jaw_right`), giving 99 matches when ignoring case. The mapping
receipt is `facial-name-compatibility.json`. However,
`export_seduxtress_eve.py` currently exports body controls and selected extra
correctives rather than the complete face set. Inspect the exact released
meshes before deciding which outfit updates are needed. Name matches are only
an initial compatibility check, not proof that every expression deforms well.
Any new facial export must preserve topology, fitting, existing morphs and
default appearance, with explicit neutral/reset and unsupported-face behavior.

The exact Eve v1.4.0 release readback now confirms this limitation in all six
variant meshes. Each contains 22 morph targets (six body controls and sixteen
hand correctives), with zero matches to the 99 facial curve names. The source
containers match the retained release audit. A CSS-only expression selector
would therefore do nothing on these released meshes. Support needs derived
Eve assets with the facial shapes retained, plus CSS/CINE runtime controls.
Keep old releases functional and show an unsupported state for missing shapes.
Evidence: `work/cine-animation-port/eve-release-meshes.json` and
`facial-release-audit.json`.

All 21 source expressions have constant curve values across their duration.
Their values range from -0.07407111 to 1.0; do not clamp authored negative
weights to zero. The source `PT_Netural_EVE` has seven nonzero curves, so it is
a named expression, not a reset instruction. Reset must restore the face state
that preceded this feature. On expression changes, clear the previous owned
expression channels before applying the next one, without clearing body or
hand morphs. Name compatibility still requires deformation review, including
eyelids, lashes, lips, teeth and tongue.

No new asset has been cooked or installed, and no CSS/CINE runtime integration
has been implemented by this task yet. Questman's stopping-transition report is
now part of the animation goal; the lighting/oil/Scyther requests remain queued.

## Next checks

Resolve the two extended-skeleton sources, inspect the remaining support assets,
and build full-motion retarget candidates at their appropriate rates. Preserve previous released idle
IDs, implement shared catalog fallback without changing outfit containers, and
validate CSS and CINE against the same resolved catalog. Track visual and live
acceptance separately from successful decode/import.

# Eve movement delivery

The user clarified that the quota priority is new playable movement, not another
backup. Walk, Jog and Sprint are now cooked and installed with the native
per-outfit animation selectors. Custom idle and beacon playback remain later
work within the full CSS v1.0.0 scope. This is a review build, not release acceptance.

## Assets and cadence

`work/anim13` contains production M3 assets: 19 sequences and three directional
BlendSpaces under `/Game/CSS/Eve/Anim/`. Each selector offers Default and
Eve (Stellar Blade). Existing outfit/variant IDs remain unchanged.

P1 phase correction is complete. Twelve whole-key shifts pass fresh raw-pose
readback against their source indices, including zero-offset and rejection
fixtures. The previous build-only checkpoint in `eve-animation-sources.md` is
historical. M2 was the phase-only control, not the shipping candidate.

M3 uses the source game's authored peaceful BlendSpace speed anchors: walk
150 cm/s, run 500 cm/s and sprint 700 cm/s. Rates account for each directional
clip's duration. The provisional M1 foot-speed estimates are superseded.
Near-zero samples retain their directional gait instead of blending a seven-second
idle into the movement clock. The runtime returns stationary characters to the
game's idle. Side/back sprint uses directional run fallback clips.

Fresh compressed component readback passes nine cases and 1,305 frames through
the accepted post-process rig. Clock checks use actual interpolation weights and
per-animation weighted rates, with the original 0.0001 tolerance. Directional
blending can differ from the nominal forward cadence, about 10.46% in the sampled
right sprint. Sampled fitted walk, left jog and sprint renders were inspected;
these stills do not establish full motion or weapon-contact acceptance.

## Cook and package gates

UE helper build, fresh readback, cook and native core build all exit zero.
The three focused portable animation/data suites pass. The cooker accepts
non-additive sequences and plain two-dimensional BlendSpaces with compatible
skeletons, finite positive rates, and no root motion or timed notifies.

The package round trip preserves accepted export/bulk payloads byte-for-byte,
including mesh, materials and rig. All 22 new animation export payloads match
their cook output after IoStore conversion. Package headers are reconstructed
by the converter, so byte equivalence is asserted for payloads, not headers.
Manifest changes are limited to the three animation definitions and container
checksums. `work/anim13/pack/verification.json` records exact package hashes.

## Installation and remaining live checks

Normal QuitGame completed before installing the trio and
`css_core-movement1.dll`. Personal state is preserved across the replacement.
Backups and deployment receipts are in `work/anim13/live` and
`backups/packages-0002`. Steam launch succeeded, and the new core answers status
requests with movement diagnostics. CSSX remains disabled.

Movement maintenance now runs every 33 ms on the existing game-thread tick.
Status exposes update count and last/mean/max milliseconds. Boot measurements
are not active-playback performance evidence.

Pending: live selector/choice persistence, linked-layer gait flag freshness under
the override, transitions back to Default and game montages, passive in-world
video and user review. Preserve V44 binding, original proportions, hair 200/24/0,
accepted body physics and the -3 cm display offset. Do not re-enable CSSX or use
DLL hot reload for this validation.

## Review findings and fixes

The user reported unsteady diagonal footing across all three gaits and made
movement stability mandatory for the alpha release. Do not publish M3 as an
alpha with this defect listed as accepted. The requested delivery is now
CSS v1.0.0-alpha.1 plus a separate Eve technology-preview sample.

The feminine idle failed because the animation graph's IsMovingOnGround flag
was false while standing. Three stationary live samples had zero velocity,
that flag false, and the player's movement-component IsMovingOnGround() true.
The controller now queries the movement component directly. Installed movement2
reports engaged idle after restart. The footer status label moves from y=953 to
y=998, below the contextual hint row at y=947. Steam capture shows the feedback
in its separate lower row.

S1 corrects the retargeted leg trajectories in work/anim14. Forward source clips
keep feet close to the sagittal plane, while M3's fitted FK output widened and
crossed their paths. Source-matched two-bone solutions preserve fitted thigh and
calf lengths and all original local translations, scales and other bone tracks.
Only thigh/calf/foot rotations on both sides change. Source knee planes and foot
rotation deltas drive the result; the validated whole-key phase shifts remain.
The combined lateral foot span changes from 36.09 to 13.35 cm for forward walk,
52.83 to 13.47 cm for jog, and 46.08 to 12.64 cm for sprint. These are trajectory
measurements, not proof of live acceptance. All 19 clips pass fresh raw readback,
and all nine timed component cases pass. Cook/package and user review follow.


## S1 accepted for alpha (2026-09-21)

Cooking and package readback passed. The package preserves 217 accepted export
and bulk payloads and adds 22 animation packages under /Game/CSS/Eve/Anim.
Installation used a normal game restart; CSSX remains disabled.

The passive 30-second recording is work/anim14/live/stable-world.mp4. Its 59
status samples include custom walk, jog, sprint and feminine idle with no
animation errors. The player also opened inventory during the recording; only
the world portions are movement evidence. Sampled world frames were inspected.
The live active-blend read confirms BS_S1_Sprint; successive reads are not an
atomic snapshot. The user then confirmed: "yeah its good now".

This accepts the reported sideways-footing correction for alpha. Preserve S1,
the original proportions, accepted hand rig, hair 200/24/0, body physics and
-3 cm visual offset. It does not establish exhaustive weapon/combat coverage.
Custom Eve idle, weapon hiding and beacon playback remain unfinished.

Reproduction: prepare_stable.py, stable_eve.py and package_stable.py in
tools/authoring-probes/animations. Work receipts are in work/anim14, including
trajectory-report.json, create-result.json, readback-result.json, cook-exit.json,
pack/verification.json and live/deployment.json. Native production build at
work/alpha1 passed with CSS_INVENTORY_DEV and CSS_TRANSITION_TESTS disabled.

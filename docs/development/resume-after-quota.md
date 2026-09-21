# CSS pause and restart handoff

Paused on 2026-09-21 at the user's explicit request to preserve the remaining
weekly quota. Do not resume development, builds, deployment or game testing
until the user asks to resume. The full CSS v1.0.0 objective is unfinished.
This is a user pause, not a completed goal or a technical blocker.

## Delivered baseline

CSS v1.0.0-alpha.1 and the separate Eve Black Pearl sample are published:
https://github.com/ngodn/CustomShellSystem/releases/tag/v1.0.0-alpha.1

Tag `v1.0.0-alpha.1` points to `84cb7ee11868ed8eb2a23b8875d6ce7ad25fc550`.
The release is a prerelease; stable latest remains v0.3.2. Downloaded release
ZIPs were compared byte-for-byte against local artifacts. Full receipts and
the bounded production gameplay coverage are in [alpha delivery](alpha1-delivery.md).
Do not repeat the completed release work or replace that tag.

Last verified installed core was `css_core-1.0.0-alpha.1.dll`, with Eve Black
Pearl and saved `idle=css.feminine`, `walk=eve`, `jog=eve`, `sprint=eve`,
`beacon=original`. Subsequent idle work made no game deployment. The game was
not queried or stopped for this pause; verify its state when testing resumes.
The final production recordings covered jog/sprint, idle, attacks and beacon
travel. Walk was accepted in the earlier S1 review, not re-observed in those
short production recordings. Do not claim exhaustive weapon/gait coverage.

Keep the accepted original body proportions, V44 hand rig, 379 raw bones plus
nine virtual bones, heel supports, 3 cm visual ground offset, accepted body
motion, and hair stiffness 200/damping 24/gravity 0. The user accepted corrected
S1 walk/jog/sprint. Do not reopen those fixes while working on custom idle.

## Current branch and ownership

Repository: `/home/eins0fx/development/mods/msII/CustomShellSystem`
Branch: `nextgen100`. Inspect HEAD and status before resuming because another
agent may be developing CSSX in the same repository.

Recent CSS/Eve commits:

- `e8763e4`: fitted I1 idle creation and reusable leg trajectory helper.
- `9f18907`: I1 saved playback, visual review and upstream renderer support.
- `5b7ee0e`: gentler I2 head gesture, authoring and validation.

`533e49e` is a separate CSSX handoff clarification already present in history.
Do not revert it. Untracked `catalog/`, `docs/development/non-css-modding-guide/`
and `extensions/core/` are unrelated to the idle checkpoint. Do not stage,
delete or overwrite them as part of this work.

## I1 and I2 idle candidates

The source is Stellar Blade's `P_Eve_Peaceful_Idle01`, seven seconds, 211 keys
at 30 fps. I1 fits the legs while preserving proportions and other motion.
Its footprint is narrower, but a downward head gesture sweeps the ponytail
too far backward. The sweep exists before secondary motion: all 29 upstream
hair local rotations remain exactly at bind over 841 samples. Inherited
head/neck motion and secondary motion both contribute.

I2 keeps 45 percent of the head-chain rotation excursion from the first pose,
changing only `neck_01`, `neck_02` and `head`. Hands, stance, timing, local
translations/scales and all unrelated frame data are unchanged. Maximum local
gesture excursion drops from 19.482 to 8.767 degrees. The final ponytail chord's
maximum inclination from vertical drops from 52.176 to 44.673 degrees. This
is a geometric offline measure, not collision or live-game acceptance.

Both revisions pass creation, fresh-load raw readback and compressed timed
playback at center/corner blend inputs. Each playback case has 841 samples
over two loops, with normalized clock error below 0.000012. Render replay
position error is below 0.000404 cm; original blend and protected existing
CSS asset hashes remain unchanged. I2's footwear height range matches I1.

Artifacts:

- `work/anim15/`: I1 source fit, raw/compressed results, upstream isolation,
  106 rendered frames, front stills, `idle-preview.mp4` and `idle-sheet.jpg`.
- `work/anim16/`: I2 equivalents, `gesture-report.json`,
  `gesture-comparison.json`, `idle-preview.mp4`, `idle-comparison.mp4`.
- In the comparison, I1 is left and I2 is right. User style review is pending.
  No reply has been received and neither candidate has live acceptance.
- The old `work/anim15/render-upstream/` report has a misleading generic scope
  label from an exploratory copied recording. Use `render-before/` instead.

Public candidate assets are `/Game/CSS/Eve/Anim/AN_I1_Idle`, `BS_I1_Idle`,
`AN_I2_Idle`, `BS_I2_Idle`. Private sequences are
`/Game/CSS/AnimLab/RT_I1_Idle` and `RT_I2_Idle`. They are saved editor assets,
not cooked, packaged or deployed.

Tracked authoring tools live in `tools/authoring-probes/animations/`:

- `prepare_idle_gesture.py`: generates I2 data with strict unrelated-frame
  equality checks. Its recorded invocation is in [animation sources](eve-animation-sources.md#i2-gentler-idle-gesture).
- `idle_eve.py`: create/readback modes, `CSS_IDLE_REVISION=I1` or `I2`,
  `CSS_ANIM_WORK` set to the corresponding work directory. It rejects
  overwriting assets/results and checks protected asset hashes.
- `render_eve.py`: default final pose; `--pose upstream` isolates the recorded
  input without downstream hand morphs and labels its report correctly.
- Recorded build, create, readback and render command arrays are inside each
  work directory. Preserve completed evidence; use a new short work directory
  for a changed candidate rather than deleting successful receipts.

## Unfinished idle-layer prototype at the pause

Two new tracked source files, `CSSIdleLibrary.h` and `.cpp`, introduce an
**editor-only** `CreateIdleLayer` helper. Matching copies are in:

`CSS-eins0fx-collections/tools/CSSAuthoring/Source/CSSAuthoring/`

The helper copies the accepted `ABP_Secondary` to an isolated
`/Game/CSS/AnimLab/ABP_Idle...` asset. It adds a dynamic `CSSIdleSequence`
reference and `CSSIdleEnabled` flag, then a sequence player and Boolean blend
before the existing secondary rig chain. Entry blend is 0.18 seconds; exit
blend is zero; the idle child resets on activation. Defaults are disabled/null.
These are prototype controls, not a shipped or finalized modder contract.

**Only the C++ helper build has run.** The helper function has never been
called. No idle-layer Blueprint has been created, compiled, saved, evaluated
or cooked. No native CSS runtime integration or weapon hiding was written.
Do not mistake the successful editor module build for a passing graph test.

Build evidence: `work/anim17/build-command.json`, `build.log`,
`build-engine.log`, `build-exit.json` (exit 0). The UE build shell session
`61585` was polled to terminal exit 0. All other processes started for this
idle work also reached terminal state. There is no outstanding authoring,
rendering, encoding or build process to wait for or restart.

Source/project copy hashes at pause:

- CPP: `e277141d158a0a4df832b0627944c6debe8997aba69718ec32845b91ab5278e4`
- Header: `500c65e015a42eee442177fffaaa2b319ee7fa8f8d09e751754804b75c2f3e5e`

The immediate next code correction, identified but **not written**, is an
invalid-sequence guard. The current Boolean feeds the blend directly. Enabling
it with a null sequence could select a reference pose. Add a valid-object
condition before exposing this interface to the runtime. The pinned engine
has `UKismetSystemLibrary::IsValid(const UObject* Object)` and
`UKismetMathLibrary::BooleanAND(bool A, bool B)` for graph construction.
Also check the variable-read helper's reconstruction/pin behavior by actually
creating the graph; that behavior is untested.

## Next implementation and validation steps

1. After explicit user resume, inspect repository changes and the pending I2
   style response. Keep that review separate from runtime correctness.
2. Finish the null-sequence guard, build, then create a fresh isolated idle
   layer. Save/read back it with all existing asset hashes protected.
3. Add an offline component test of inactive pass-through, activation and
   playback phase, immediate release, reactivation/reset and null clip. Use
   a distinct base animation so an unchanged default pose cannot pass as idle.
   Verify all bones when initially inactive; compare unaffected bones during
   transitions while accounting for retained secondary-motion history.
4. Only after those checks, wire selected idle sequences into the native CSS
   runtime. Existing metadata already promises `clip`, optional `by_weapon`
   sequence paths and common `hide_weapons`; do not silently reinterpret them
   as BlendSpace paths. Unsupported/missing assets must fall back gracefully.
5. Implement eligibility and ownership for both pose and weapon visibility.
   Account for grounded state, movement intent/velocity, aiming, attacks,
   parry/block, damage, death, menu/travel, pawn/mesh replacement, outfit switch
   and disabling CSS. Restore only CSS-owned visibility changes and preserve
   weapon gameplay state, traces and collision. Never hide only one weapon
   component accidentally, restore someone else's state or fight another writer.
6. Build and run focused runtime tests, cook/read back the isolated assets,
   then package/deploy via normal game restart. Record actual world transitions
   and inspect the exact release candidate, not just a menu pose. Measure
   performance with the idle inactive and active.

`work/anim16/game-graph-summary.json` traces all 62 serialized ABP_Player nodes
and records the decoded source hash. The main animation slots are followed by
aiming/late update, pose offsets, hand correction, melee assist, Control Rig
and head tracking. A dynamic montage alone would not prove a correct final
unarmed pose. This is why the post-process idle branch is being prototyped.
The main game animation graph must remain functional when the branch is off.

If reconsidering a montage implementation, inspect the recorded findings
first: UE 5.6.1 dynamic montage playback stops existing montages in its group,
and CSS `Appearance::ready_to_apply()` rejects any active montage. An owned
idle montage would need coordinated release. Do not repeat this investigation
or change the game graph's hand IK globally as a shortcut.

## Remaining full v1.0.0 work

Custom Eve idle and weapon restoration are still unfinished. Beacon kneel/rise
must preserve the game's timed blocking, fade and travel events. Broader
weapon/pose/combat and visual checks remain open. Follow up the Harbinger
toggle's contextual help placement. The standardized modder kit, base assets,
guide and recorded Blender-to-Unreal-to-pak/ucas/utoc workflow belong under
`/home/eins0fx/development/mods/msII/CSS-Modding`, with the requested public Git
repository. Plugins should encode the proven workflow after stabilization.
See [work queue](../work-queue.md) and linked feature documents for prior work.

## CSSX and operating constraints

CSSX v1 plus Cheat Menu v1 are separate-agent work under `extensions/core`.
The [handoff](../cssx/standalone-v1-agent-prompt.md) requires standalone operation
and coexistence with unchanged CSS alpha. Residual FPS loss is unresolved;
the old HUD optimization did not close it. Do not re-enable legacy CSSX or
touch the separate agent's source as part of Eve work. Coordinate before any
future shared game deployment.

Use pinned UE 5.6.1 (editor C++20/Python 3.11), Blender 5.2.2 and native C++23.
UE4SS remains `3.0.1-1111-g97b7e501` NO AOB, with the intentionally retained
header pin recorded in `native/ue4ss-runtime.json`.

Keep generated work, logs and backups under the workspace, with short names.
The recorded commands use bwrap with workspace writes and workspace-backed
`/tmp`. Game deployment/runtime-request writes were authorized previously.
Asset paths use `/Game/CSS/`; do not introduce `/CSSAuthoring/` asset paths or
UUID directory names. Normal restarts only: prior DLL hot reload caused an
animation-worker crash. Prefer Steam screenshots or passive game-window video;
do not inject movement controls while the user plays. Commit by context and
state test limits honestly. The explicit user pause overrides autonomous work.

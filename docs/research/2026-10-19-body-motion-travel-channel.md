# Body motion travel channel and soft limits

Brief: the "Why the new physics feels stiff" and "Motion-feel plan" sections of [UUEPBS compatibility and motion feel](2026-10-19-uuepbs-compatibility-and-motion-feel.md). Branch `feature/uuepbs-body-sliders`. Nothing here was installed or run in the game.

Status on 19 Oct 2026:

- Investigation of where the limits live and how the solver applies them: done, from the rig builder sources and the recorded Peony evidence.
- Travel channel schema, saved state, presets and native adapter: done on the branch, host tested. Windows core compiles. Not installed.
- AnimBP inputs, rig graph change, soft limit, UI and recooks: designed here, not done. They belong to the editor/cook lane and a later UI pass.
- Measurement comparison: planned here with exact files and commands, not run.

## Short answer

The seven limits are two seven-entry member variables on the shared Eve body ControlRig, `Limits` (radians) and `MaxTravel` (cm), not AnimBP inputs and not graph constants. The solver takes the smaller of the angle limit and the angle at which the helper's weighted skin would move `MaxTravel` cm, projects the orientation back onto that cone, and deletes the outward angular velocity. That is a hard, perfectly inelastic stop. For the chest and glutes the cm cap is the one that binds (3.83 and 8.01 degrees), which is exactly the Iris Out peak recorded after 1.5.0. Firm and Natural both saturate it, which is why no slider setting changes the feel near the limit.

A `travel` channel in degrees per region is now in the control schema and native adapter. It writes `CSSBodyRegionTravel` when a rig has it and quietly does nothing on rigs that do not, which is every rig shipped today. The soft limit belongs in the rig step, as a spring-back force over the last 25 percent of the travel, with the hard clamp kept only as a safety net.

## Where the seven angle/travel limits live

| Question | Answer | Source |
| --- | --- | --- |
| Rig asset defaults? | Yes. `Limits[7]` and `MaxTravel[7]` are ControlRig member variables on `/Game/CSS/eins0fx/EveSB/CR_Body`, the body rig shared by every Eve variant. | `tools/diagnostics/prepare_body_limit_trial.py`, `docs/research/2026-10-12-peony-v2-gameplay.md` ("Actual animation poses expose the wider movement envelope") |
| Stored where inside the asset? | Twice: the class default object and the Blueprint variable defaults. Editing only the CDO is undone on reload, because RigVM restores both arrays from the variable defaults. The 1.5.0 candidate edits both. | same gameplay record, `peony-v2-body-limits1` |
| AnimBP inputs? | No. The AnimBP (`/Game/CSS/eins0fx/EveSB/ABP_Secondary`) forwards frequency, damping ratio, motion amount, enabled, region switch, geometry and contacts. No limit reaches it. | `control-convention.md` "CSS ControlRig body inputs", `compare_body_rig_response.py` (the inputs it sets) |
| Graph constants? | No. The step reads `g.at('Limits', i)` and `g.at('MaxTravel', i)` per helper per substep. | `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools/controlrig_body_step.py`, lines 53 to 62 |
| Anything else involved? | `WeightedSupportRadii[7]` (cm), the measured weighted skin radius of each helper, turns `MaxTravel` into an angle. It is a measurement, not a tuning value. | `CSS-Mod-Authoring/docs/next-gen-body-travel.md` |

The 1.5.0 package (`peony-v2-motion-package1`) changed exactly 14 numeric CDO values, seven `Limits` and seven `MaxTravel`, with bytecode and hierarchy unchanged. A read-only probe of the running game later confirmed all fourteen on the active rig. "Seven limits" means seven angle/travel pairs.

### Values and the limit that actually binds

Effective cap = min(`Limits[i]`, 2 asin(`MaxTravel[i]` / (2 (R_i + 0.01 cm)))), with R_i the weighted support radius (chest 22.41/22.48 cm, glutes 7.15/7.23, thighs 3.63, belly 1.99). Computed from the recorded radii:

| Region | 1.5.0 angle | 1.5.0 travel | 1.5.0 effective | binds | Pre-limit angle | Pre-limit travel | Pre-limit effective |
| --- | ---: | ---: | ---: | --- | ---: | ---: | ---: |
| brust001/002 | 6 deg | 1.5 cm | 3.83 / 3.82 deg | cm cap | 28 deg | 11 cm | 28 deg |
| butt001/002 | 10 deg | 1 cm | 8.01 / 7.92 deg | cm cap | 24 deg | 5 cm | 24 deg |
| thigh_twist_02_l/r | 3 deg | 0.5 cm | 3 deg | angle | 10 deg | 2 cm | 10 deg |
| belly | 5 deg | 0.5 cm | 5 deg | angle | 15 deg | 1.5 cm | 15 deg |

V42 used the same envelope as 1.5.0. "Pre-limit" is the broad envelope from `gemini-work/build_cr_body_jiggle.py`, which was cooked into Eve and Genessa before 1.5.0.

The computed 3.83 and 8.01 degrees match the recorded Iris Out peaks for both Firm and Natural on the restored rig (`peony-v2-clip-response1`). Both presets run into the cap on demanding motion. In default idle, Firm stays at about 0.83/2.15 degrees and never touches it.

A second, smaller reason the sliders cannot add freedom on Eve: all four Eve body controls declare `motion_amount` max 1 (`work/anim14/.../eins0fx.seduxtress/manifest.json`). The built-in Bouncy, Soft and Earthquake rows ask for 1.35 to 4.2, which get clamped to 1. On Eve, every preset above Natural differs only in frequency and damping, inside the same cap.

## How the solver clamps

From `controlrig_body_step.py` (V5 and later, which is the production body rig's lineage):

1. Integrate angular velocity for one fixed 1/120 s step (spring toward the animated target, exponential damping, inertial and gravity torque scaled by motion amount).
2. Propose the new orientation and take its rotation vector relative to the animated target, length `theta`.
3. `limit = min(Limits[i], travel_cap(MaxTravel[i]))`.
4. If `theta > limit`: set the orientation to `limit` along the same axis (spherical projection), and subtract the outward component of `(omega - target_omega)` along that axis.
5. Planes and contacts then project again (`controlrig_body_planes.py`).

So it is a hard clamp, and perfectly inelastic: the helper loses all outward speed in one 8.3 ms step and the spring then pulls it back from rest. Visually that is a stop, not a bounce, which is what "stiff rather than smooth" describes. The clamp also hides the frequency and damping sliders: once a move saturates it, the peak and the stop look the same at every setting.

For comparison, UE 5.6.1 `FAnimNode_SpringBone` (`AnimNode_SpringBone.cpp`, lines 174 to 186) also clamps by projection and recomputes velocity from the clamped position, so its clamp is inelastic too. But the 2.0.x packages players remember shipped with the clamp off (2.0.1 K220/D9.5, about 2.4 Hz and zeta 0.32; 2.0.2/2.0.3 recipes at zeta 0.65 to 0.78). Their motion was bounded by damping alone, so it overshot and settled. The Better Jiggle reference (K120/D7.5 with a 1.5 cm clamp) is lively for the same reason: a low damping ratio with the clamp rarely reached.

## Travel channel

### Meaning

`travel` is the largest angle, in degrees, a region may swing away from its animated pose. One value per control, applied to every region the control owns. In the rig, a positive travel replaces both `Limits[i]` and the `MaxTravel` cm cap for that region. Zero means "use the rig's own limit", so a rig recooked with the new inputs at their defaults behaves exactly like 1.5.0.

Replacing the cm cap, not taking the minimum with it, is deliberate. For the chest the cm cap is the binding limit, so a minimum would pin travel at 3.83 degrees and the channel would do nothing. That makes each package's declared `travel.max` the safety bound. An author picks it after checking garment clearance at that angle (`audit_body_motion_clearance.py` in the SeduXtress authoring tools). Skin travel at a given angle is 2 R sin(theta/2); the template table below lists it.

### Schema (done)

Recipe, on an `angular_body` control only:

```json
"travel": {"min": 2, "max": 24, "default": 4}
```

- `0 < min < max <= 45`, default inside, step 0.5 (capped by the range width). Ceiling constant `body_travel_ceiling` in `native/src/controls.hpp` and `BODY_TRAVEL_CEILING` in `tools/css_controls.py`.
- Rejected on hair rigs, springs and every other kind. A spring keeps `max_displacement` in cm.
- Without `travel` a control has four channels as before. All shipped recipes validate unchanged (checked against the Eve manifest in `work/anim14` and Genessa's `authoring/uuepbs-customize.json`: 12 body controls, all valid).
- The declared default should be the rig's effective 1.5.0 cap rounded to the step (chest 4, glutes 8, thighs 3, belly 5). The rig keeps its exact cooked cap until the player moves the slider, because CSS writes nothing until there is an override.

### Saved state and requests (done)

`ControlValue` is four floats, and a body rig already uses all four (frequency, damping ratio, motion amount, enabled). Widening it would touch every saved look and every colour path. Packing travel into the enabled channel would make old CSS builds reject the whole look (`value()` refuses channel 3 above 1). So travel is stored beside the value:

- `Customization::travel`, a `map<control id, degrees>`, serialized as `"travel": {"chest_motion": 6}` only when non-empty. Older CSS builds ignore the key, so a look saved with travel still loads there without it.
- Schema-free bound on load: 0 < degrees <= 45. `compatible_values` keeps an override only where the control has a travel range that contains it; otherwise the part falls back to its default, like any other value.
- Requests: channel 4 (`{"action":"control","control":"chest_motion","channel":4,"value":6}` or `delta` in 0.5 steps). This also covers the CSSX `customize` export, which goes through `apply_customize`. `reset_control` drops travel. Original drops all travel. A named palette drops travel for the parts it sets. Palettes do not carry travel.
- Presets: `PhysicsPreset::travel` (optional). Built-ins carry per-region travel and write it only to parts with a travel range, clamped to that range. A package preset may add `"travel": <degrees>` on such a part. `apply_physics_preset()` now holds the `physics_preset` request logic that used to be inline in `core.cpp`, so it is host tested. `matching_physics_preset()` takes an optional current travel, so the menu can show "Custom" when only travel moved.

### Native adapter (done)

Following the spring `max_displacement` pattern (a declared range, a live channel, captured originals restored on removal):

- `BodyRigSettings` gains `travel[7]`, `global_travel` and `has_travel`.
- `body_rig_settings(controls, values, authored, travel)` composes travel overrides onto the owned regions only when `authored.has_travel`. A travel-only override keeps the region's tuning. On a rig without the inputs a travel-only override is a no-op: it does not even switch region mode.
- `body_travel_support(BodyTravelFields)` reduces what reflection found to facts. Both properties absent: `Absent`, travel disabled, everything else applies. Both present as a float array of seven and a float: `Ready`. Anything in between throws like any other contract mismatch.
- `engine_dynamics.inl` `BodyRigInputs` looks up `CSSBodyRegionTravel` and `CSSBodyTravel` in the same bounded property walk, validates layout and storage, captures them with the other inputs, writes them only when both the instance and the settings have travel (so a preview instance without the inputs is left alone), reads them back, and advances `CSSBodyResetEpoch` on change like the other body inputs.
- `engine.cpp` `Appearance::customize` gets `control_travel()`, treats a travel-only part as active, includes travel in the change check (`last_travel_`), restores the captured inputs when a travel override is dropped, and reports `travel` and `body_travel_supported` in the material debug snapshot for readback.
- The SpringBone fallback (a body rig control on an AnimBP that only has SpringBone nodes) ignores travel. Travel is an angle and that path writes cm; mapping one onto the other would invent a meaning.

### AnimBP contract (editor lane, not done)

Add to each body AnimBP, beside the existing `CSSBody*` inputs:

| Variable | Type | Default | Forwarded to rig input |
| --- | --- | --- | --- |
| `CSSBodyRegionTravel` | float array, exactly 7, canonical region order | all 0 | `RegionTravel[7]` |
| `CSSBodyTravel` | float | 0 | `Travel` |

Units are degrees. Zero means the rig's own limit. Both must exist together. They are authored by the same per-package editor libraries that create the other region arrays (for example `CSS_CommanderWhite_eins0fx_P/tools/native/CWSecondaryLibrary.cpp`; the probe module `tools/authoring-probes/body-inputs/Source/CSSBodyInputs/CSSBodyInputsLibrary.cpp` shows the array pattern), with defaults exported without delta suppression as the region arrays already are.

### Rig graph change (editor lane, not done)

In `controlrig_body_step.py`, replace the limit lines with:

```
requested = UseRegionSettings ? RegionTravel[i] : Travel          # degrees
authored  = min(Limits[i], travel_cap(MaxTravel[i]))               # today's cap, radians
limit     = requested > 0 ? radians(min(requested, 45)) : authored
```

Add `RegionTravel` and `Travel` as rig member variables with zero defaults, set in both the CDO and the Blueprint variable defaults (the 1.5.0 lesson). Keep `Limits`, `MaxTravel` and `WeightedSupportRadii` unchanged so the zero path reproduces 1.5.0 exactly. Builder flag suggestion: `-CSSBodyTravelInput` on `build_controlrig_body_timed.py`, producing the next body version.

### UI (later pass, not done)

Precedent: the spring Bounce / Settle / Travel sliders in `native/src/inventory_view.inl` (`physics_sliders`, around lines 917 to 945).

- In `physics_sliders`, a body rig with `body_travel_control()` shows four sliders: Bounce (ch 0), Settle (ch 1), Amount (ch 2), Travel (ch 4, readout in degrees, value from `custom.travel` or the declared default), then the Motion switch (ch 3). The focus index needs a slot-to-channel map, because Travel is channel 4 but sits before the switch.
- Pass the current travel to `matching_physics_preset` in the row state and the preset panel, so moving only Travel shows "Custom".
- Row "Edited" state: also true when `custom.travel` has the part.
- Help text: "Travel sets how far it may swing, in degrees."
- When the worn rig lacks the inputs (`body_travel_supported` false in the snapshot), show the slider disabled with "This outfit's rig predates Travel", rather than a slider that does nothing.
- `ui_channel` counts already use `control_channel_count`, which now returns 5 for these parts.

### Physics templates (built-ins, done in code, provisional values)

Chest and glute values are the ones in the brief. Thighs and belly were not specified; Firm keeps the 1.5.0 caps (3 and 5) and the rest loosen in proportion. Skin travel is 2 R sin(theta/2) at the chest/glute radius.

| Preset | Chest deg | Glute deg | Thigh deg | Belly deg | Chest skin cm | Glute skin cm |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Firm | 4 | 8 | 3 | 5 | 1.57 | 1.01 |
| Natural | 6 | 10 | 4 | 6 | 2.35 | 1.26 |
| Bouncy | 10 | 14 | 6 | 8 | 3.92 | 1.76 |
| Soft / Saggy | 14 | 16 | 7 | 9 | 5.48 | 2.01 |
| OMG! Earthquake! | 20 | 18 | 10 | 12 | 7.81 | 2.26 |

Notes:

- Firm equals the accepted 1.5.0 envelope (3.83 rounds to 4). Natural at 6/10 is looser than what the user accepted for Natural on 12 Oct ("Motion looks right now" at 3.83/8.01). Gate it on the trace comparison and a live verdict before shipping.
- Earthquake chest 20 degrees is 7.8 cm of skin travel. Garment clearance at that angle is unmeasured.
- These rows only take effect on parts that declare `travel`, and only on rigs with the inputs. Until a recook, nothing changes in game.
- Eve's `motion_amount` max 1 still clamps the upper presets' third channel. Retuning should consider raising it to the 5 the schema allows, measured with the same traces.

### Migration for shipped outfits

| Case | Result |
| --- | --- |
| Recipe without `travel` (every package today) | Validates. Four channels. Presets write no travel. Nothing changes. |
| Recipe with `travel`, rig without the inputs | Accepted. `has_travel` false, travel skipped, other channels apply, travel-only override is a no-op. The UI should show the slider as unavailable. |
| Rig recooked with the inputs at default 0 | Identical to 1.5.0 until a travel override is written. |
| Rig with a half-built pair | Refused at resolve time, like any other contract mismatch. Author error, caught by the editor probe before cook. |
| Look saved with travel, opened in an older CSS | Older `Customization::parse` ignores `travel`. The look loads without it. |
| Look saved without travel, opened now | Loads with empty travel. |
| Profile restored onto a variant without travel, or a narrower range | That part's travel drops to the author default; the rest of the look is kept. |

Per package (Eve, Unholy Genessa, Commander White, tHorny Proxima): add the AnimBP pair and the rig inputs, recook the body rig and AnimBP, add a `travel` range to each body control with the default equal to the rounded effective cap, then verify with the fixtures below and the live probe that read the 1.5.0 limits (`sample_secondary_rigs.py` already reads `Limits` and `MaxTravel`; add `RegionTravel` and `Travel` to its `FIELDS`).

## Soft limit design

Goal: reaching the limit never stops the helper dead. It should slow over the last 25 percent of the travel and spring back.

### Recommended: spring-back zone in the rig step

In `controlrig_body_step.py`, before `omega = omega + force * dt`, compute against the committed orientation:

```
rel   = log_rotation(difference(old, target))       # relative rotation vector
theta = |rel|, u = rel / theta                      # axis (fallback (1,0,0) like today)
L     = limit (as above), z = SoftZone (0.25), s = (1 - z) * L
x     = clamp((theta - s) / (z * L), 0, 1)
v_out = max(0, dot(omega - target_omega, u))

force += -u * wL^2 * (theta - s)^2 / (z * L)  when theta > s   # restoring, zero slope at the knee
force += -u * 2 * zetaL * wL * x * v_out                        # extra damping, outward only
```

- The restoring term is zero with zero slope at the knee, so there is no acceleration step there. It reaches wL^2 zL at the limit and absorbs wL^2 (zL)^2 / 3 of kinetic energy (per unit inertia) between knee and limit.
- Size wL so that a helper swinging at the main spring's own full-amplitude speed (v = 2 pi f L) stops before L: wL >= sqrt(3/2) v / (zL) = sqrt(1.5) * 2 pi f / z, about 4.9 times the region's angular frequency, independent of L. Cap it at 1.5 / dt = 180 rad/s for semi-implicit stability at 120 Hz. At Firm chest (2.6 Hz) that is 80 rad/s, wL dt = 0.67.
- zetaL = 0.7 for the zone damping, on the outward component only, so the return swing is not slowed.
- Keep the existing hard projection after it, as the safety net for inputs faster than v. Count how often it engages.
- Add `SoftZone` as a rig input with default 0.25. `SoftZone = 0` reproduces today's hard clamp exactly, which gives a same-rig A/B and a rollback.
- The new terms depend only on committed state and the interpolated target, so the fixed clock, whole-step commit and preview-restore rules are unchanged.

### Fallback: output soft clip

If the fixtures show the hard projection still engaging often under Iris Out, map the displayed angle through `theta' = theta` below the knee and `s + zL tanh((theta - s) / (zL))` above it, with the state itself clamped at `s + 2 zL`. Displayed motion then never exceeds L and its slope at the state clamp is 0.07, so a residual stop is invisible. It cushions instead of rebounding, so it is the second choice.

### Verification with the timing fixtures

Reuse `probe_controlrig_body_timing.py` (37 fixtures, 15/30/60/120/144/240 FPS, irregular frames, owner/origin/scale, reset, hitch, duplicate evaluation) and `probe_controlrig_body_travel.py` (14 travel fixtures, including tight limits at every rate), reviewed by `review_controlrig_body_timing.py` and `review_controlrig_body_travel.py`, all in `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools/`. Add:

- A `ClampContacts` counter in the rig, incremented on committed steps where the hard projection engages, read like `ResetCount`.
- Soft-limit fixtures: smooth and piecewise translation with a tight `RegionTravel` (2 degrees) and with each template's travel, `SoftZone` 0 and 0.25, at all six rates.

Gates, fixed before running as the earlier reviews did:

1. `theta <= L + 1e-6` on every frame.
2. Unchanged tolerances: 0.25 degree piecewise and 0.3 smooth versus the 120 FPS reference, 0.05 at aligned times, 0.02 owner/origin/scale, 0.001 duplicate, settle below 0.1 degree, zero pivot and scale drift, preview state unchanged.
3. `SoftZone = 0` with all travel 0 matches the 1.5.0 rig within 0.001 degree on all existing fixtures.
4. Largest single-step change in outward relative angular speed inside the outer 25 percent band: at most a quarter of the `SoftZone = 0` value on the same fixture.
5. `ClampContacts` zero on the smooth translation fixtures at Firm and Natural, and reported (not gated) for Iris Out.
6. Knee continuity: no step in angular acceleration larger than the spring term alone produces when crossing `s`.

Then the skinning check (`review_body_travel_mesh.py`) at the template angles, because positive travel replaces the cm bound.

## Measurement plan: 1.5.0 vs pre-limit vs 2.0.x SpringBone (not run)

Three sources, one metric set.

### Offline rig traces (editor commandlet, no game)

Existing pieces in `tools/diagnostics/`:

- `compare_body_rig_response.py`: `evaluate()` runs a transient ControlRig with the AnimBP's public inputs and a preset per region. Its mode `current` is the pre-limit rig (`/Game/CSS/SeduXtress/CR_Body`, broad limits). `current_old_limits` sets 1.5.0's `Limits`/`MaxTravel` on the same rig; the 1.5.0 cooked candidate matched it in six cases. `v42` is the retained V42 rig.
- `check_body_clip_response.py`: feeds every source key of retained clips, mapping all 379 bones, and writes `response.json` with per-frame `samples_degrees`.
- `verify_body_clip_response.py`: the independent checker.
- Stage template: `work/three-outfit-rig-physics/peony-v2-clip-response1/` (`editor/` with `Content` linked read-only to `CSS-eins0fx-collections/tools/CSSAuthoring/Content`, `PeonyLighting.uproject`, and `command.json`, the exact bwrap plus `UnrealEditor-Cmd -run=pythonscript` invocation).

New script to write: `tools/diagnostics/compare_body_motion_feel.py`, importing `evaluate`, `PACKAGES`, `NAMES` from `compare_body_rig_response.py` and the pose loader from `check_body_clip_response.py`:

- Modes `pre_limit` (= `current`), `v150` (= `current_old_limits`), and once a rig has the inputs, `v150_travel` (template travel written to `RegionTravel`) and `v150_soft` (`SoftZone` 0.25).
- Presets Firm, Natural, Bouncy, Earthquake, with Eve's clamp on motion amount (max 1) and, as a second set, unclamped (max 5).
- Clips: `/Game/CSS/Locomotion/Eve/idle/AN_Default`, `/Game/CSS/Locomotion/Studies/L5X/AN_IrisOut`, `/Game/CSS/Locomotion/Eve/catwalk/AN_EveCatwalk`, `/Game/CSS/Locomotion/Eve/photo/AN_Walk`. There is no retained run clip; run comes from the live capture below.
- Rates 30, 60, 120. Output `response.json` with per-frame helper angles plus each mode's effective cap per region.
- Companion checker to write: `tools/diagnostics/verify_body_motion_feel.py`, modelled on `verify_body_clip_response.py` (exit 0, unchanged inputs, case coverage, 379 mapped bones, finite samples, one reset per case, zero motion when disabled, `v150` peaks at or below the computed caps).

Commands (from `CustomShellSystem/`):

```sh
S=work/three-outfit-rig-physics/body-motion-feel1
mkdir -p "$S"/{tmp,cache,ddc,user,config-home,editor}
cp work/three-outfit-rig-physics/peony-v2-clip-response1/editor/PeonyLighting.uproject "$S/editor/"
cp -P work/three-outfit-rig-physics/peony-v2-clip-response1/editor/{Content,Binaries} "$S/editor/"
python3 - "$S" <<'PY'
import json, sys, subprocess, pathlib
stage = pathlib.Path(sys.argv[1]).resolve()
old = 'work/three-outfit-rig-physics/peony-v2-clip-response1'
argv = json.loads(pathlib.Path(old, 'command.json').read_text())
src = str(pathlib.Path(old).resolve())
argv = [a.replace(src, str(stage)) for a in argv]
argv = [a.replace('check_body_clip_response.py', 'compare_body_motion_feel.py') for a in argv]
(stage / 'command.json').write_text(json.dumps(argv, indent=2) + '\n')
rc = subprocess.run(argv, stdout=open(stage / 'engine.log', 'w'), stderr=subprocess.STDOUT).returncode
(stage / 'exit.json').write_text(json.dumps({'returncode': rc}) + '\n')
PY
python3 tools/diagnostics/verify_body_motion_feel.py "$S"
```

Check `command.json` by eye before running: the clip-response command binds `XDG_CONFIG_HOME` to a path on `/run/media/eins0fx/theark`, which the replace does not move.

### 2.0.x SpringBone reference (pure Python, no editor)

New file: `tools/diagnostics/springbone_reference.py`, a line-for-line port of `FAnimNode_SpringBone::EvaluateSkeletalControl_AnyThread` (UE 5.6.1 `AnimNode_SpringBone.cpp`, lines 100 to 190): fixed 1/120 s substeps, `a = K error - D v`, the damping cutoff at 1/dt, `ErrorResetThresh`, the optional clamp with velocity recomputed from the clamped position. Input: the component-space target position of each helper per frame, taken from the same clip pose evaluation (add a `--dump-targets` option to `compare_body_motion_feel.py` that writes `targets.json`). Cases: 2.0.1 K220/D9.5 unclamped, the 2.0.3 package recipe values (read them from that package's cooked `ABP` spring nodes, not from memory), and Better Jiggle K120/D7.5 with a 1.5 cm clamp. The port has no owner velocity, because clip poses carry no root motion. Note that in the summary.

### Live traces (needs a dev core and the user's go)

The `motion_sample` request exists only in a core built with `-DCSS_INVENTORY_DEV=ON` (`native/src/core.cpp`, `sample_motion()`). Installing that core is an install step, so it waits for the user. Request, written to `<mod>/request.json` with `tools/css.py` `atomic`:

```json
{"id": 1760900001, "action": "motion_sample", "seconds": 10,
 "bones": ["brust001", "brust002", "butt001", "butt002", "spine_04", "pelvis"]}
```

The result lands in `<mod>/runtime/motion-sample.json`: one frame per tick, at most 1800 frames and 12 bones, each bone as `GetSocketTransform` in `TransformSpace 3` (parent bone space). The helper tracks are locally constant in the retained clips (`peony-v2-animation-tracks1`), so the angle between a helper's sampled local rotation and its Motion-off local rotation is its secondary deflection. Capture order per preset: Motion off (baseline), then Firm, Natural, Bouncy, Earthquake, each with the same route walked, run and Iris Out. The user places the character and picks the route. Not in a menu.

### Analysis

New file: `tools/diagnostics/analyze_body_motion_feel.py`, reading `response.json`, the SpringBone output and any `motion-sample.json`, and writing one table per clip and preset:

- peak angle and peak skin travel (2 R sin(theta/2) for the rig, displacement for SpringBone), so rotation and translation compare on one axis;
- time within 10 percent of the cap (saturation dwell);
- largest single-frame change in angular speed (the hitch) and RMS jerk (third difference over dt cubed);
- settle time after the input stops, to 0.1 degree;
- dominant oscillation frequency.

Expected from the existing numbers: pre-limit Firm and Natural peak at 20 and 28 degrees under Iris Out; 1.5.0 saturates at 3.83/8.01 with long dwell and large single-frame speed drops; the 2.0.1 SpringBone overshoots without a stop. The travel and soft modes should land between, with the hitch metric near SpringBone's. The user's live verdict stays the gate for retuning the templates.

## Changes on this branch

| File | Change |
| --- | --- |
| `native/src/controls.hpp`, `controls.cpp` | `travel` range on body rigs, channel 4, `Customization::travel`, `control_travel`, `body_travel_control`, `body_travel_support`, travel-aware `body_rig_settings`, palette/reset/restore handling |
| `native/src/physics_presets.hpp`, `.cpp` | Per-region travel in the built-ins, package preset travel, `physics_preset_travel`, travel-aware `matching_physics_preset`, `apply_physics_preset` |
| `native/src/core.cpp` | `physics_preset` request uses `apply_physics_preset` |
| `native/src/engine_dynamics.inl` | Optional `CSSBodyRegionTravel`/`CSSBodyTravel` resolve, capture, apply, read-back |
| `native/src/engine.cpp`, `engine.hpp` | Travel in `Appearance::customize` composition, change tracking, restore and debug snapshot |
| `tools/css_controls.py` | Optional `travel` range on body rigs, refused elsewhere |
| `tests/control_tests.cpp` | 78 new checks: legacy recipe unchanged, schema bounds, composition, old rig no-op, global seeding, requests, round trip, older saves, restore and compatibility, palettes, presets, package presets, `body_travel_support` |
| `tests/test_css_controls.py` | `test_body_rig_travel_is_optional_and_bounded` |
| `docs/control-convention.md` | "Body travel (candidate, 2026-10-19)" |

Test results on 19 Oct 2026, host build `build/uuepbs-host`: ctest 28 of 28 suites pass (before and after). `css_control_tests` 470 checks pass (392 before). Python `unittest discover -s tests -p 'test_*.py'` 91 tests pass (90 before). The Windows core (`build/uuepbs-windows`, clang-cl, UE4SS SDK d7e7826d) compiles and links with only pre-existing warnings. It was not installed.

## Open points

- The control convention still states body bounds of 0.5..6 Hz, 0.1..2 and 0..1. The native parser and the validator accept 0.2..10, 0.01..3 and 0..5, and the built-in presets use motion up to 4.2. One of them should change. This branch does not touch it.
- Positive travel replaces the skin-travel bound. Each package's `travel.max` has to come from a clearance check at that angle, and the templates' upper rows are unmeasured against garments.
- Natural 6/10 loosens the look the user accepted. Ship it only after the traces and a live check.
- Fixture count is not frame cost. The soft zone adds a log_rotation and a few vector ops per helper per substep; measure with `measure_body_graph_cost.py` before shipping.

## Status after the editor pass (19 Oct 2026)

### Built

| Part | Where | State |
| --- | --- | --- |
| Rig step and builder | `tools/authoring/body-rig/` (copied from the SeduXtress library, which is unchanged) | `-CSSBodyTravelInput` authors `BodyAngularV11`: V10 plus `RegionTravel[7]`, `Travel`, `SoftZone` (0.25) and `ClampContacts`, 1276 nodes (V10: 1162). `-CSSBodyHierarchyFrom=<rig>` duplicates an existing rig, keeps its hierarchy and members and rebuilds only the graph. |
| Combined AnimBP | `tools/authoring-patches/body-travel-inputs.patch` against the CSSAuthoring module | Adds and wires `CSSBodyRegionTravel`/`CSSBodyTravel` when the rig has both. Compiled in a private stage, `ABP_CSS_ControlRigProbeCombinedV4` authored on V11. |
| Fixtures and gates | `probe_controlrig_body_timing.py`, `probe_controlrig_body_softlimit.py`, `review_controlrig_body_softlimit.py` | 37 timing fixtures, 234 soft-limit fixtures, run. |
| UI | `native/src/inventory_view.inl` and helpers | Travel slider, disabled on rigs without the pair. Host and Windows builds pass. |

Two deviations from the design text, both deliberate:

- The zone damping is integrated exactly over the step (the same exponential form as the main damping) instead of as an explicit force. At the 180 rad/s cap, 2 zetaL wL dt is 2.1, and an explicit step would reverse the velocity instead of damping it.
- The design's restoring term is applied with the penetration clamped to the band width, so a limit that shrinks under a committed state cannot produce an unbounded push.

Two findings along the way: the V40 mesh now has nine virtual bones (388), so a fresh import no longer gives the 379-bone hierarchy; and rebuilding the members from the measured JSON moves the chest by one float ulp (hierarchy-derived defaults are written at 12 digits). Both are why recooks duplicate the current rig.

How to run (from `CustomShellSystem/`, stage `work/body-travel-softlimit1`, everything outside the stage read-only under bwrap):

```sh
R=tools/authoring/body-rig; S=work/body-travel-softlimit1
python3 $R/run_stage.py $S $R/build_controlrig_body_timed.py -CSSBodyTravelInput \
  -CSSBodyHierarchyFrom=/Game/CSSAuthoring/TransientProbes/CR_CSS_BodyAngularV10 --log build-v11
python3 $R/run_stage.py $S $R/probe_controlrig_body_timing.py -CSSBodyVersion=10 -CSSBodyLabel=v10 --log timing-v10
python3 $R/run_stage.py $S $R/probe_controlrig_body_timing.py -CSSBodyVersion=11 -CSSBodySoftZone=0 -CSSBodyLabel=sz0 --log timing-sz0
python3 $R/run_stage.py $S $R/probe_controlrig_body_timing.py -CSSBodyVersion=11 -CSSBodySoftZone=0.25 -CSSBodyLabel=sz25 --log timing-sz25
python3 $R/run_stage.py $S $R/probe_controlrig_body_softlimit.py -CSSBodyVersion=11 --log softlimit
python3 $R/review_controlrig_body_timing.py $S --version 11 --label sz25
python3 $R/review_controlrig_body_softlimit.py $S
```

The combined AnimBP check runs in a second stage with the patched module built (`run_stage.py --module`, `check_combined_travel_inputs.py`).

### Gate results

| Gate | Result |
| --- | --- |
| 1. theta <= L + 1e-6 every frame | Pass. 13417 frames x 7 helpers on each 37-fixture run and 146898 frames on the soft-limit fixtures. Largest excess 4.6e-7 rad (SoftZone 0), 8.7e-9 rad (soft-limit fixtures). |
| 2. Unchanged tolerances | The unchanged timing review passes 27566 of 27566 checks on V11 at SoftZone 0 and at 0.25. On the soft-limit fixtures 1061 of 1100 checks pass: pivot, scale, unit quaternions, clock and preview checks all pass; 39 timing comparisons against the 120 FPS twin fail, 20 at SoftZone 0 and 19 at 0.25. They are the Bouncy, Soft and Earthquake rows, the bounce motion at 144/240 FPS and smooth input at 15/30 FPS (worst 1.19 degrees, Earthquake bounce, SoftZone 0). The hard clamp fails the same cases, so this is the tolerances meeting much larger motion than the 37 fixtures, not the soft zone. Four cases fail only with the zone (Soft bounce 15/30, Soft smooth 30, tight smooth 15, worst 0.42 degree): a stiff band reacts more to coarse input sampling. |
| 3. SoftZone 0, travel 0 equals the old rig | Pass, bit for bit: 93919 of 93919 output transforms identical to a V10 rerun, and the V10 rerun is identical to the archived 20 Sep V10 run. |
| 4. Band speed change at most a quarter of SoftZone 0 | Fail on all 12 applicable 120 FPS fixtures. Ratio 0.45 to 1.32 (for example Natural bounce 1.50 vs 1.76 rad/s, tight smooth 0.31 vs 0.53). |
| 5. No hard contacts, Firm and Natural smooth, SoftZone 0.25 | Pass, 0 at every rate (12 of 12). |
| 6. No acceleration step at the knee | 12 of 15 pass. Fails on Earthquake bounce (2428 vs 2121 rad/s^2 below the knee), Earthquake piecewise (643 vs 589) and tight piecewise (80 vs 65). |

Hard projection contacts, SoftZone 0 vs 0.25: 126 vs 0 on the 37 fixtures, 5726 vs 862 over the soft-limit fixtures. At 120 FPS: tight smooth 325 vs 0, Bouncy smooth 76 vs 0, Soft smooth 121 vs 5, Natural bounce 17 vs 0, Earthquake bounce 101 vs 28. Earthquake piecewise and smooth go from 0 and 1 to 1 and 4: the elastic push sends a lightly damped helper back harder. Firm never reaches 75 percent of its travel on these fixtures. Settle to 0.1 degree after the input stops is the same or up to 0.27 s shorter with the zone, except Soft bounce (2.58 to 2.82 s); Earthquake does not settle within 4 s either way (damping ratio 0.06 to 0.10). The 8 Hz case, where the zone stiffness reaches its 180 rad/s cap, never gets deep into the band (peak 0.85 of L), so the cap's stability is not exercised by these fixtures.

Why gate 4 fails: at the speeds these fixtures reach, a 25 percent band is crossed in about two 120 Hz steps (Natural bounce enters at 1.57 rad/s with a 0.026 rad band). Even a constant deceleration would need the whole band to spread the stop over four steps, and the quadratic push puts most of it near the limit, so the worst step is about half the entry speed. The hard clamp's own worst step is also less than the full speed, because the stop usually lands mid-step. The design's wL sizing stops the helper, but not gently enough for this gate. Gate 6 fails for the same reason: a fast crossing covers the first part of the quadratic in one step.

Exploratory, not a gate: SoftZone 0.5 at 120 FPS on the same fixtures gives a band speed ratio of 0.20 to 0.54 on 6 of the 12 cases (Bouncy smooth 0.20, tight smooth 0.24, Natural bounce 0.45), and still 0.62 to 1.17 on the other six (Soft rows, Bouncy and Earthquake bounce, Earthquake smooth). Contacts drop further (tight smooth 0, Bouncy smooth 0, Soft smooth 5, tight bounce 92 vs 236). It is a runtime member, so this needs no rebuild.

### Open

- The soft limit as designed does not meet gates 4 and 6. Options, in order of cost: a wider `SoftZone` (0.5 measured above), a band whose push grows less steeply (linear above the knee with a matched damper), or the output soft clip from "Fallback". Pick after the trace comparison, not from these synthetic fixtures alone.
- Template travel and tuning retune after the trace comparison (`compare_body_motion_feel.py`, still to write) and the user's live check. Natural 6/10 is still looser than the accepted 1.5.0 look.
- Per-outfit recooks: Eve's `CR_Body`/`ABP_Secondary`, Unholy Genessa, Commander White and tHorny Proxima each need the rig rebuilt from their current rig with `-CSSBodyHierarchyFrom`, the AnimBP rebuilt with the patched builder (or their own library, for example Commander White's), a `travel` range in their recipes, and the live read-back probe (`sample_secondary_rigs.py` with `RegionTravel`/`Travel` added).
- Frame cost of the 114 extra nodes is unmeasured (`measure_body_graph_cost.py`).
- The UI uses a degree sign in the readout; the game font has not been checked for it.

## Status after pass three (19 Oct 2026, night)

Fable's decision: SoftZone default 0.5, a cubic push from the knee, and the band per template (Earthquake 0.6). Built and measured in `work/body-travel-softlimit2` (fixtures, logs, reviews) and `work/body-travel-softlimit2-module` (AnimBP check). `run-all.sh` and `run-module.sh` in those stages are the exact command sequences.

### What changed

| Part | Change |
| --- | --- |
| Rig step | Push `(4/3) wL^2 d^3 / w^2` above the knee (d the depth, w = zL). Zero value, slope and curvature at the knee; the 4/3 keeps the quadratic's energy to the limit (`wL^2 w^2 / 3`), so the wL sizing is unchanged. The outward damping is still integrated exactly. The zone is read per region (`RegionSoftZones[7]`) when region settings are on, else `SoftZone`; both default 0.5. 1290 nodes (V11 pass two: 1276). |
| Templates | `soft_zone` on each built-in body preset: 0.5, Earthquake 0.6 (`physics_presets.cpp`). `physics_soft_zones()` gives each active body part the band of the preset its sliders match, else 0.5. It is derived, not saved; a package preset or a hand-tuned part gets 0.5. |
| AnimBP | `body-travel-inputs.patch` also adds `CSSBodyRegionSoftZones` (seven floats) and `CSSBodySoftZone`, defaulted from the rig, wired when the rig has both. The combined fixture gains `region_soft_zone`. |
| Native | Optional soft zone pair beside the travel pair: absent is accepted (the rig keeps its own band), a half pair is refused (`body_soft_zone_support`). Values are bounded to 0..0.9 before writing and read back. |
| Fixtures | The soft-limit set runs each template at SoftZone 0 and at its own zone (`sz50`, Earthquake `sz60`). The review applies the new pass criteria below; gate 2 is reported, not part of the verdict. |

### Gate results (pass criteria fixed before the run)

| Gate | Criterion | Result |
| --- | --- | --- |
| 1 | theta <= L + 1e-6 every frame | Pass. Largest excess 4.6e-7 rad (SoftZone 0, 13417 frames x 7 helpers), 8.7e-9 rad on the 146898 soft-limit frames, below the limit on the SoftZone 0.5 timing run. |
| 2 | Timing tolerances (reported) | The 37 timing fixtures pass 27566 of 27566 checks at SoftZone 0 and at 0.5. Soft-limit fixtures: 1064 of 1100; the 36 failures are 20 at SoftZone 0 and 16 with the zone, the same large-motion rows as pass two (bounce at 144/240, smooth at 15/30). |
| 3 | SoftZone 0 bit-identical | Pass. 93919 of 93919 output transforms identical to a V10 rerun and to the archived 20 Sep V10 run. |
| 4 | Band speed ratio < 0.6 on 10 of 12 | **Fail: 5 of 12.** Under: Bouncy smooth 0.20, tight smooth 0.28, Bouncy piecewise 0.45, Natural bounce 0.48, tight piecewise 0.54. Over: tight bounce 0.63, Soft smooth 0.64, Soft bounce 0.64, Soft piecewise 0.87, Earthquake smooth 0.93, Bouncy bounce 1.18, Earthquake bounce 1.21. |
| 5 | ClampContacts near zero (<= 5), Firm/Natural/Bouncy smooth | Pass. 0 at every rate on all 18 fixtures. |
| 6 | Knee continuity, non-Earthquake | **Fail: 13 of 14.** Tight bounce 332 vs 294 rad/s^2 below the knee. Earthquake (reported): smooth passes, piecewise 898 vs 751 and bounce 2751 vs 2255 fail. |

Hard contacts: 126 to 0 on the 37 timing fixtures; 5726 to 954 over all soft-limit fixtures. At 120 FPS: tight smooth 325 to 0, Bouncy smooth 76 to 0, Soft smooth 121 to 5, Natural bounce 17 to 0, tight bounce 236 to 65, Earthquake bounce 101 to 22. Settle times are the same or shorter except Soft bounce (2.58 to 2.83 s). On the 37 timing fixtures with travel 0, SoftZone 0.5 moves the output by up to 1.72 degrees against SoftZone 0, which is the band acting on helpers that enter the outer half.

The cubic changes almost nothing against the quadratic at the same width. Gate 4 ratios at 0.5, cubic vs pass two's exploratory quadratic: Bouncy smooth 0.20 vs 0.20, tight smooth 0.28 vs 0.24, Natural bounce 0.48 vs 0.45, Soft smooth 0.64 vs 0.76, Bouncy bounce 1.18 vs 1.03. What decides gate 4 is which rows still reach the limit: every row over 0.6 except Earthquake smooth peaks at 1.00 of L and still makes hard contacts. On those rows the input outruns the band's energy budget, so the stop happens at the hard projection anyway, and the band's stiffer final push (the cubic peaks 4/3 higher at L) adds to the speed change rather than spreading it. The bounce motion (2.5 Hz piecewise, a velocity reversal every 0.2 s) and the lightly damped Soft and Earthquake rows are those cases.

Not run: a different curve. The open options stay the ones listed in pass two (a band with more energy, such as a linear push matched at the limit, or the output soft clip). They need Fable's call and a rebuild.

### AnimBP check

`check_combined_travel_inputs.py` on V11 pass three (`ABP_CSS_ControlRigProbeCombinedV5`): defaults `CSSBodyRegionTravel` seven zeros, `CSSBodyTravel` 0, `CSSBodyRegionSoftZones` seven 0.5, `CSSBodySoftZone` 0.5; the V10 rig still authors without either pair; all ten fixtures transfer every frame (241, 241, 241, 241, then 121/241/481 for `region_travel` and for `region_soft_zone`, whose distinct per-region values change at one second).

### Open

- Gates 4 and 6 do not pass. The decision on the curve or the width goes back to Fable, with the numbers above.
- Frame cost of the soft zone is still unmeasured (`measure_body_graph_cost.py`).
- The live check, the trace comparison and the template retune, as before.

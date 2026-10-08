# Cooked secondary-motion ownership

This inventory describes the four coordinated trial containers. It does not
accept visible motion, contact quality or frame cost. The user has accepted the
facial/visibility correction and cross-character Iris Out faces/eyes with
attack/aim interruption. The full regression matrix remains open.

## Inputs and repeatable check

`work/three-outfit-rig-physics/secondary-bindings5/report.json` pins hashes for the
three final decoded outfit containers, shared catalog and nine-variant manifests.
The report contains 18 normal/facial mesh records, ten animation graphs, five
playback adapters and eight Control Rig assets.

```sh
python3 tests/secondary_bindings_tests.py
python3 tools/diagnostics/audit_secondary_bindings.py \
  --catalog work/three-outfit-rig-physics/catalog-namespace3/shared-inline.json \
  --outfits work/three-outfit-rig-physics/catalog-namespace3/migrated-outfits.json \
  --readback work/three-outfit-rig-physics/eve-final-container1/decoded.json \
  --readback work/three-outfit-rig-physics/commander-final-container1/decoded.json \
  --readback work/three-outfit-rig-physics/genessa-face-namespace-pack2/decoded.json \
  --output /tmp/css-secondary-bindings.json
```

The output path must not already exist. Fourteen negative/control tests pass.
All five adapters retain their source graph's CSS control types/defaults, rig
classes, input binding identities, pose-transfer flags and explicit bone
transform targets/spaces. Generated node GUIDs may differ. Swapping two
same-type inputs is rejected. Repeated endpoint entries inside a single hair
node are distinct from two nodes claiming the same named bone.

## Region owners

All paths below are relative to `/Game/CSS/eins0fx/`.

| Outfits and region | Cooked owner | Driven region and contact inputs | Reset/control evidence | Remaining proof |
| --- | --- | --- | --- | --- |
| All six Eve outfits, body | `EveSB/CR_Body` | Seven helpers: chest pair, glute pair, thigh-twist pair, belly; seven contact-frame entries | `ResetEpoch`, `UpdateSerial`; regional frequency/damping/amount/enabled plus morph-dependent offsets, inertia and contact axes | Continuous motion, frame-space correctness, contact fit, tick/reset behavior |
| Both Genessa variants, body | `UnholyGenessa/CR_Body` | Same seven helper names; character-owned asset and shape inputs | Same exposed inputs as Eve; source and performance adapter agree | Same motion/contact checks on both body variants and ornaments |
| Commander White C4/E9, body | `CommanderWhite/Rig/CR_BodyB3` | Seven helpers; smaller authored angular/travel limits; all seven `ContactActive` defaults false | Regional settings and reset/update fields retained in `ABP_BodyHairB4` and its adapter | Determine effective runtime contact settings before changing colliders |
| Eve, hair | `EveSB/CR_Hair` | 36 point entries, 29 distinct named hair bones; 14 contact entries | `ResetEpoch`, `UpdateSerial`, gravity/stiffness/damping/enabled; 1/120-second authored step | Root anchoring, actual weighted strands, update order, clearance and cost |
| Genessa, hair/hood-associated chains | `UnholyGenessa/CR_Hair` | Same named-point inventory as Eve; this alone does not prove which visible parts are weighted to it | Same exposed reset and hair controls | Map chain weights to actual visible geometry before altering this solver |
| Commander White, hair | `CommanderWhite/Rig/CR_HairH4` | 36 point entries, 29 distinct named bones; character-specific contact radii/margins | Reset/update fields and hair controls; 1/120-second authored step | Head/shoulder clearance, reset behavior and measured cost |
| Eve and Genessa, hand correction | Their own `CR_Hand` | Compiled hand/articulation/clearance solver; no `BoneNames` array | `Enabled`, `InputIsV43Compatible` bound on both normal and playback graphs | Literal bone targets and parent propagation are not decoded by this check |
| Eve Bikini/Knitwear/Alice, feet | `ABP_BikiniFeet2` or `ABP_KnitFeet1` | Left/right foot additive rotations in bone space | Targets/spaces retained by corresponding playback adapters | Posed shoe clearance, contact phase and native IK interaction |
| Genessa, wings | `ABP_Wings2` and `ABP_Genessa` | Four `ug_wing_*` bones; additive bone-space rotations | All four transform targets and spaces retained | Gait-driven values, hinge motion, nearby cloth and transition timing |
| Cloth | Per-mesh `ClothingAssetCommon` objects below | Independent simulation meshes and per-cloth Physics Assets | Cooked ownership and vertex counts below | Pin weights, morph/simulation correspondence, active colliders and visible contact |

### Cooked pose-link order

The direct `AnimInstance` subclasses store their node fields in the serialized
`ChildProperties` order. The walker follows pose `LinkID` values from the output,
rejecting cycles, invalid indices and disconnected writers. Local UE 5.6.1
`AnimBlueprintGeneratedClass.cpp:500-516` builds `AnimNodeProperties` by iterating
the node fields; `AnimNodeBase.cpp:185-201` resolves a pose link through that
array. This is a graph-path check, not a runtime timing measurement.

| Graph family | Input-to-output bone writer order |
| --- | --- |
| Eve main | Body, hair, hand correction |
| Eve Bikini/Knitwear/Alice | Body, hair, hand correction, right foot, left foot |
| Genessa | Body, hair, hand correction, lower-left wing, lower-right wing, upper-left wing, upper-right wing |
| Commander White | Body, hair |

Every output-connected branch in each of the five playback adapters retains its
source's ordered bone writers. A playback branch that bypasses them or changes
their order fails the check. This does not determine which branch is selected at
runtime, whether the solver is enabled, or when its event graph updates inputs.

No overlapping **named `BoneNames`** writers were found in any of the ten graphs.
This is not proof that all writers are disjoint: hand-rig literal targets,
parent propagation, live scheduling and cloth render transfer remain to
be examined. The cooked reader exposes RigVM literal property descriptions and
serialized size, but not the literal values needed to resolve every hand target.

Eve/Genessa list the two thigh contact frames in the opposite left/right order
to Commander White. That is an observation, not a diagnosed defect. Compare
actual reference transforms and the model's local contact geometry before
changing either mapping.

All three hair assets declare 64 contact iterations. This is a solver default,
not a measured per-frame execution count or permission to increase it. The
effective loops and their cost require runtime or VM evidence.

## Cloth inventory on facial replacements

Counts below are LOD0 physical simulation vertices, not render vertices or
performance measurements. Each normal mesh and its facial replacement is recorded
separately in the JSON report. There are nine variants and 18 distinct mesh paths.

| Variant | Cloth assets | LOD0 simulation vertices | Cloth collision asset |
| --- | ---: | ---: | --- |
| Eve Black Pearl | 0 | 0 | None |
| Eve Prototype | 1 | 18 | `EveSB/.../PA_PTailRear` |
| Eve Skin Suit | 0 | 0 | None |
| Eve Vacation Bikini | 0 | 0 | None |
| Eve Casual Knitwear | 1 | 1,679 | `EveSB/.../PA_KCloth4` |
| Eve Midsummer Alice | 1 | 624 | `EveSB/.../PA_ACloth3` |
| Genessa, Eve body | 6 | 3,420 | `UnholyGenessa/PA_Body`, `PA_SkirtFit` |
| Genessa, Genessa body | 6 | 3,420 | `UnholyGenessa/PA_Body`, `PA_SkirtFit` |
| Commander White E9 | 1 | 4,893 | `CommanderWhite/.../PA_ClothC13` |

The JSON records exact paths, cloth names, used bones, per-LOD vertex/index counts
and render section counts. A component Physics Asset is not automatically the
collision source of every attached cloth asset. Preserve this distinction when
testing a contact repair.

## Next checks

1. Extend the accepted facial/visibility and cross-outfit checks to both Genessa
   body variants, CINE, form changes and doubles.
2. Hand source targets and 42 full W3 editor-component samples are now recorded
   in the hand study. Gameplay contact and scheduling remain separate checks.
3. Record simulation space, anchor/weight coverage and reset behavior for the
   region owners above. Capture matched enabled/disabled motion before proposing
   a geometry, weight or collision change.
4. Keep the full regression and performance matrix open. This structural audit
   does not replace those tests.


## Actual weighted surfaces

`skin-influences1` records all 18 normal/facial editor meshes, 570 render sections
and 11,934,678 render vertices across those meshes. This total counts both versions
of each character and is not a simultaneous gameplay vertex count. The UE probe
exits 0; all weights are normalized, indices valid and source assets unchanged.
The optional coverage output leaves the existing `SnapshotMesh` output unchanged.

The independent full-array W3 check matches all 25 material sections. Each of
the nine normal/facial pairs has identical body/hair/foot/wing region summaries.
This tests actual nonzero influences, rather than assuming every bone in a
section's bone map affects a vertex. Counts summed across bones are influence
counts, not unique affected vertices. Ancestor coverage is labeled potential
because it does not prove the rig's propagation behavior.

| Region | Weighted surfaces | Consequence for further work |
| --- | --- | --- |
| Eve hair, all six outfits | Hair and ponytail material sections only | Check those surfaces against head/shoulder/clothing clearance |
| Commander White hair, C4/E9 | Main hair, both bangs, flyhair and both tail sections | Preserve these six material groups and their weight gradients |
| Genessa hair rig, all four meshes | No direct or descendant render influences; no named hair bones in any of the six cloth assets' used-bone lists | The existing hair rig has no identified skin/cloth consumer. Check live execution and cost before removing it |
| Genessa body | Skin, harness, bodice trim, bodice and skirt | Fitted trim already has body-helper weights; missing weights alone do not explain a preview/world difference |
| Genessa wings | Each wing bone maps to its corresponding wing section, each with cloth | Keep bone gait motion and cloth render transfer distinct |
| Commander White body | Body plus fitted garments/accessories and simulated skirt | Cloth sections need simulation-transfer checks in addition to skin weights |
| Eve body | Skin and the corresponding fitted costume, plus cloth ribbon/tail/knit sections where present | Preserve body-following weights when diagnosing cloth |

`probe_skin_influences.py` runs against the isolated combined source project in
`skin-influence-module1`. The report pins each UAsset and result hash.
`tools/diagnostics/summarize_skin_influences.py` joins those captures with the
cooked owner inventory; `regions.json` retains every section/bone result.
`independent-validation.json` records the full-array cross-check and nine pair
comparisons. Config-write warnings come from the read-only editor sandbox.
Nothing was saved back to an asset or installed in the game.

The apparent left/right difference in Eve/Genessa thigh contact frames is also
consistent between their model helper records and rig arrays. Commander White
uses its own matching arrays. Do not swap those names based on naming alone:
opposing-surface contact and helper-parent frames are different roles.

Cleanup removed 157,390,336 logical bytes of regenerable DDC from the three
completed hand-component runs. Reports, source, recipes, outputs and binaries
remain. See `cleanup-hand-component-cache1.json` for observed space changes.

## October 9: hair lifecycle and Genessa's unused solver

`hair-lifecycle2` exercises the three existing editor hair rigs directly in
UE 5.6.1 CL44394996. All 27 cases and 3,447 samples pass: head motion at 30, 60
and 120 fps, disabled input, re-enable, explicit reset, a 1,000 cm owner teleport
with reset, a 200 ms hitch and repeated evaluation without an update-serial
change. The original rig files retain their hashes.

Each uninterrupted two-second case executes 240 fixed steps at every tested
rate. Disabled cases execute zero steps and preserve every incoming hair pose.
Reset, teleport, hitch and re-enable each produce the expected second reset.
Repeated evaluation neither advances the step counter nor changes the result.
Every non-hair bone retains its supplied transform on every sample: 350 for
Eve/Genessa and 366 for Commander White. These are the rigs' own hierarchies,
not mesh skeleton sizes. Output transforms remain finite.

At matched timestamps, the largest 30-versus-120 fps hair-position difference
is 0.02353 cm; the largest rotation difference is 0.11666 degrees. The
60-versus-120 maxima are 0.00471 cm and 0.03782 degrees. These quantify this
synthetic head turn only. They do not establish garment clearance or contact
quality during gameplay.

Direct editor execution medians at 60 fps are approximately 0.95 ms for Eve,
0.94 ms for Genessa and 1.15 ms for Commander White; disabled cases measure
about 0.02 ms. This includes the Python-to-RigVM call and was not captured in
an isolated gameplay benchmark. Do not convert these values into an FPS gain.
Raw sample timings, source hashes and the independent step-count/rate comparison
are retained in `hair-lifecycle2/report.json` and `summary.json`.

The first harness run used the C++ name `GetBoneKeys` as a Python method. UE's
`RigHierarchy.h` exposes it under `ScriptName="GetBones"`; that run stopped
before any cases and was rejected. The corrected harness uses `get_bones()`.
The public [Control Rig scripting documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/control-rig-python-scripting-in-unreal-engine)
is a general reference; the harness uses the locally verified 5.6.1 API.

The live read-only `hair-live2` capture finds Genessa Eve-body facial mesh using
`ABP_Genessa`, with the hair rig enabled and its node alpha at 1. The same rig's
`TotalSteps` advances from 21,766 to 22,209, while `ResetCount` remains 4.
Mesh and post-process instance remain stable and saved settings are unchanged.
The sampled attachments use ordinary head/body/weapon/root sockets, with no
named hair socket. This follows the four-mesh weight audit finding no hair
render or cloth consumer. An earlier capture expired during an animation
change and was rejected rather than combined with the stable samples.

A Genessa-only private candidate disables the unused solver's `CSSEnabled`
default in both the normal graph and facial adapter. Genessa's catalog has no
hair-motion control. The initial in-memory comparison permits only that boolean
change; body, hand, wing and facial properties are unchanged. Sources remain
immutable. Candidate authoring lives in the Genessa mod's `work/hair-solver-idle1`.
Fresh reload and the Windows-target cook pass. `hair-solver-component1` compares
both graphs against all four normal/facial meshes in three supplied poses,
including seven control/reinitialization frames for each hair-enable state.
All 336 samples preserve non-hair bone transforms and the 16 corrective curves
and their rendered morph weights exactly. All 24 comparisons change hair bones,
so the enabled/disabled comparison does exercise the solver. This is not a
continuous cloth or rendered-contact test.

`hair-solver-pack3` preserves all 265 payload files except the two graphs'
UAsset/export pairs. Independent cooked graph readback permits only the false
`CSSEnabled` default (serialized as an omitted zero-valued bool) and the CDO's
editor transaction flag. Every other decoded field, including graph bytecode,
matches. The shared catalog updates one playback and two facial revision
bindings to recognize the new Genessa container. Shared cooked payloads and
facial tracks remain byte-identical. All 855 combined package checks, 422 native
catalog checks, 1,386 outfit checks and 26 metadata-file readbacks pass.

The first package comparison rejected the false-default omission and editor
flag. Its four exact differences remain in `hair-solver-pack1/differences.json`.
The second package used V11 metadata instead of the established V8B format and
failed the outfit catalog check before installation. The final recipe uses V8B
and refreshes all three revision bindings; it does not weaken catalog checks.
Gameplay appearance and matched runtime performance remain unverified for this
optimization. See the [outfit recipe](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/authoring/inactive-hair-solver-notes.md).

Installed after the checks with the game closed; all six package files were
independently rehashed. The core, Eve/CW, profiles and CSSX/CINE files are
unchanged. The outfit's `hair-solver-pack3/installed.json` records the receipt
and rollback transaction. In-game confirmation is pending.

The rejected first and second package stages/containers were removed after
the third package passed and was installed. Their graph readbacks, differences,
failure logs and metadata remain. Both removed stages were byte-identical to
the retained third stage. This removes 1,005,428,968 logical bytes, but the
immediate free-space counter did not change; no measured recovery is claimed.
The outfit's `work/hair-solver-cleanup1.json` records each removed file/hash.

Cleanup removed 1,199,552,670 logical bytes of task-local derived data from the
completed weight audit and rejected first lifecycle harness. Reports and inputs
retain their hashes. The immediate filesystem free-space counter did not change,
so no measured space-recovery claim is made. Receipt:
`cleanup-hair-audit-cache1.json`.

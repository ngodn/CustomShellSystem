# Commander White blood and Peony revealed skin

Status, October 17, 2026: CW 0.1.1 and its Shared Assets 1.0.1 metadata update
are packaged as local releases at the user's later request. The cooked CW clothing and skin
pass the native blood ladder, color and profile checks. Peony's reproduced dark
bands come from the equipped weapon's shadow; no Peony asset repair is justified.
Validation limits are recorded below. Working branch:
`investigate/cw-blood-peony-skin`.

## Baseline and authorization

The user approved the installed CSS 1.0.1 baseline and gave go for the first
Commander White take. Preserve the selected scene, character and saved state.
Ask before staging any DLL or package. Frozen release archives remain unchanged.

`work/cw-blood-peony-skin/baseline-02/receipt.json` records 14 file identities,
including the installed runtime, all three relevant container trios and the
metadata reader. Every input retained its hash across this passive audit.
The audit sent no game requests. Metadata was unpacked with the existing repak.

| Installed item | Observed identity |
| --- | --- |
| CSS core | `css_core-1.0.1.dll` |
| Shared metadata | `css.locomotion`, version `1.0.1` |
| Commander White metadata | `eins0fx.commanderwhite`, version `0.0.6-dev` |
| Eve metadata | `eins0fx.seduxtress`, version `1.4.0` |

The outfit version fields differ from the task's frozen CW 0.1.0 and Eve 1.5.0
release names. `release-comparison.json` establishes byte-identical installed
`.utoc` and `.ucas` files against those frozen release ZIPs. The metadata `.pak`
files differ only in manifest version fields: CW 0.0.6-dev versus 0.1.0,
and Eve 1.4.0 versus 1.5.0. All other embedded files match exactly
(`metadata-comparison.json` and `frozen-metadata/`). These captures cover the
frozen cooked payloads with the installed metadata and CSS 1.0.1.

Python is 3.14.7. `mise current` and `mise ls` were checked; the project specifies
Python 3.14, C++23 and .NET 10. Runtime source and the core DLL are unchanged.
The user approved installing and testing the candidates. Four package files
were installed during a normal game restart, as recorded below.

## Established context

- The Seductress controlled comparison changed only BRM red on the same MID.
  A white channel caused broad coverage there. This is a precedent to test,
  not a measured cause for Commander White.
- Peony V2 supersedes the old 5,328-face boot recipe. Its documented coverage
  is 4,944 body triangles in skin aliases 38/39/40, plus stocking section 42.
  The current metadata has 13 removable groups. The older 18/19/37 section
  IDs must not be reused. Verify these numbers against installed readback.
- Reflected `CustomPrimitiveData` contains serialized defaults. It cannot
  establish the live renderer's primitive inputs.
- Material static switches are compiled choices, per Epic's
  [parameter documentation](https://dev.epicgames.com/documentation/unreal-engine/material-parameter-expressions-in-unreal-engine?lang=en-US).
  A runtime scalar write is not a test of a different static permutation.

Read the task's linked Seductress investigation/fix, CW source/repair lineage,
and Peony V2 source/material/package/gameplay/lighting records before extending
this work. The old lighting acceptance did not identify or repair a cause.

## Measurements and matrix

| Case | Status | Evidence |
| --- | --- | --- |
| Installed container/runtime identities | Recorded | `baseline-02/receipt.json` |
| CW clean and 0.2/0.5/0.8/1.0 blood, then clear | Symptom reproduced; idle pose varies | `cw-before-02/` |
| Stock Genessa and Black Pearl blood controls | Baseline captured; post-install four-strength ladders retain material identity and frozen pose, then clear to 1.0 | `controls-before-02/`, `controls-after-01/` |
| User-requested Unholy Genessa reference | Both Genessa and Eve variants complete the native ladder and clear; saved state restored | `unholy-controls-01/` |
| CW per-material cooked masks and static switches | Decoded; red-only live comparison passes | `cw-cooked-01/`, `cw-red-comparison-07/` |
| Peony each removable group, all off, preview/world | 13 single removals, all shown/off and re-show captured in world and CSS preview | `peony-inspection-03/`, `peony-preview-02/` |
| Peony four palettes, glow off/on, all combinations | Eight all-off palette/glow cases captured in world and preview; 65,536 combinations pass production visibility resolution and save round-trip, not exhaustive GPU rendering | Same folders; `peony-combinations.log` |
| Peony restored face/UV/material/normal identities | Exact skin corner equivalence with Skin Suit | `peony-surface-comparison.json` |
| Skin Suit and Black Pearl same-region skin controls | World controls in four directions, plus CSS preview controls with the same harsh shading; exact Peony/Skin Suit live skin bindings | `peony-inspection-03/`, `peony-preview-controls-01/` |
| CW combined clothing/skin candidate | Installed; ten front/rear ladders across full outfit, coat hidden and exposed skin; all four strengths clear to 1.0 | `cw-package-04/`, `cw-acceptance-01/`, `cw-acceptance-wide-01/` |
| CW palettes and displayed color swatches | Original plus five palettes and nine swatches for each of two color controls; BRM bindings unchanged | `cw-acceptance-01/audit.json` |
| CW profile save/load | Temporary profile reload restores exact customization; existing profiles and full saved state unchanged | `cw-profile-02/` |

Evidence and scripts are under `work/cw-blood-peony-skin/`. Initial setup
failures are retained: `baseline-01` stopped because the metadata parent
directory did not exist; `cw-before-01` selected a hidden auxiliary window
with the game's application class. Both were corrected without asset or
gameplay writes. World screenshots now use native `HighResShot`; CSS preview screenshots use
`Shot showui -nosuffix`, so desktop focus does not substitute a different app.
`peony-preview-01` captured the underlying viewport before selecting the CSS
tab and is not CSS preview acceptance; `peony-preview-02` corrects this. The initial probe script import also needed the lexical repo
path because `work/` is a symlink to the evidence drive.

## Commander White measurements

All 70 live material slots retained their identities throughout the initial
blood ladder. The leotard and boots turn broadly black and glossy at strength
0.5, then return to their clean appearance after clearing. Skin stays pale.
The camera was unchanged, but the idle pose was not held for that first ladder.

The 23 effective BRM textures have entirely white or nearly white red channels:
17 are solid 255; six span 247 through 255, with at least 99.996% white texels.
The live materials have no BRM texture overrides. CW's dye surfaces target
`BaseColorMap  non VT`, not BRM. The clothing parent inherits `USE BLOOD MASK=true`
from `MI_Uber_Char`; the body parent `MI_ShellKeeper_Body_03` overrides it false.
Custom hair graphs use red as AO, so they must not receive a blanket red edit.

`cw-red-comparison-07/result.json` isolates slot 40, the leotard, on the same MID.
Both imported textures use identical green and blue bytes and the same import
method; only red differs, white versus a horizontal gradient. Pose snapshots
and camera rotations are identical in both blood captures. At native strength
0.5, the white version darkens the whole leotard while the gradient leaves a
white region at its lower front. Native mask measurements are 0.52225/0.55597
around the white capture and 0.52234/0.55639 around the gradient capture.
This confirms hypothesis 1 for the leotard and supports testing spatial masks
on the remaining garments. The gradient is diagnostic, not a release asset.
Original texture, animation pause, appearance and saved state were restored;
the native mask returned to 1.0. Earlier attempts were rejected for camera or
desktop-view changes and are not controlled evidence.

## Peony measurements

After the candidate report, the user supplied a close-up of dark torso/arm
coverage and then identified it themselves as the stocking layer: "ohhh itu
stocking wtf, ok ok la". The live status was read while Peony was selected;
no outfit, material, camera or effect was changed during this follow-up.
Treat that screenshot as a user-identified clothing-layer case, separate from
the controlled axe-shadow finding below. It does not establish approval of
the outstanding acceptance deviations.

The first world matrix covers all 13 removable controls alone, all shown and
all off. Section flags restore the covered feet when boots are hidden; material
identities remain unchanged. Camera changes during that batch prevent using
the stills as matched-view acceptance. The all-off frame does not show the CW
whole-surface coating, but that does not close the reported regional defect.

The installed Peony and Skin Suit meshes have exactly the same 185,442 skin
corners (61,814 triangles), including material identity, position, all UVs,
tangent, normal, handedness and vertex color. The comparison uses the existing
MeshExport streams and tangent decoder, with material indices normalized by
resource path. There are zero missing or extra corners. Peony covered aliases
38/39/40 contain 218/3,890/836 triangles, totaling 4,944, and retain the accepted
feet/nail skin materials. No zero tangent or normal occurs in these aliases or
the body, leg and arm regions. This rules against a unique cooked assignment,
UV or tangent defect in those regions. It does not prove live material inputs,
deformed shading, palette behavior or preview lighting are correct.

The completed world follow-up covers four views, all four palettes with glow
0/17.25, all 13 individual removals, all off, all shown and re-show. The CSS
preview repeats the palette/glow and visibility matrix on its own character
component. Body/leg/arm/covered-foot live material parents and base/BRM bindings
match Skin Suit exactly. Clothing changes can restart cloth or morph evaluation,
so not every separate visibility case has an identical pose snapshot. The
all-off palette comparisons hold the pose. No material or mesh repair is justified
by these comparisons.

The sharp world hip/thigh bands are reproduced and isolated in
`peony-axe-shadow-01`. The actual caster is the equipped Axatana axe's
`SM_Weapon` static component. Its root `WeaponMesh` skeletal component is a
separate object; the earlier root-only shadow tests did not isolate the visible
weapon. With `SM_Weapon.CastShadow=false`, the hip/thigh bands disappear while
body shadows remain; restoring true brings them back. Readback verifies the
changed flag and restoration. All five body pose snapshots match. Weapon
attachment transforms can still move, so shadow outlines between separate
captures are not pixel-identical. This is a native weapon shadow, not a skin
texture or restored-face defect. No permanent shadow suppression is proposed.

Earlier `peony-render-path-01` confirms that disabling all shadows removes the
bands. Contact shadows, ray-traced visibility, ray-traced shadow settings and
shadow cache did not identify a skin defect. Those diagnostic settings were
restored. Console-variable numerical restoration does not remove the console
priority until restart. The existing sunlight-noise investigation is related
rendering context, not proof that its workaround fixes this symptom.

The first multi-variant inspection changed Eve's remembered outfit customization
while restoring individual variants. The final restoration explicitly reselected
the original remembered Peony look before returning to CW.
`peony-inspection-03/state-restoration.json` verifies full state equality with
the saved control baseline. Later Peony-only probes and preview batches each
verify state equality directly. Input blocking is limited to captures because
CSS correctly defers outfit changes while look input is ignored.

## CW candidate and validation

The spatial leotard test `cw-spatial-comparison-02` repeats the same-MID,
same-pose and same-camera comparison using the baked candidate instead of a
simple gradient. Lower leotard coverage becomes patchy; the separate torso
material still uses its original white mask in that test. All overrides restore.

`bake_cw_masks.py` uses existing MeshExport triangle/UV readback in Blender
4.5.13 LTS. The bake combines 18/m and 110/m spatial noise at 0.82/0.18,
with a 0.44/0.60 ramp, following the Seductress recipe. Thirteen BRMs cover
boots, coat, skirt/belt, leotard, top, gloves, ruffles, lace, cap, hairband and
small metal parts. Hair graphs that use red as AO, eyes and skin are excluded.
Green/blue values are copied from the installed decoded BRMs.

Ten garment textures preserve the exact source green/blue byte arrays. Three original
4x4 constant textures need more pixels for their small UV islands and were
expanded to 1024x1024. Their constant green/blue values are unchanged, but their
array dimensions and byte counts differ. Extra texels are required to represent
spatial blood coverage on these islands. This is a documented deviation from
the task's literal byte-identical source requirement. It preserves the exact
roughness/metallic value at every source and destination texel, but cannot be
reported as array byte identity. The receipts identify these three cases.

An offline follow-up repeats the same spatial bake at the original 4x4 size,
without enlarging anything (`original-size-mask-audit.json`). Green/blue arrays
remain byte-identical in all three diagnostic outputs. The slot-16 and slot-34
textures sample zero occupied UV texel centers and bake all-zero red. The
slot-47 texture samples one occupied center, red 70, and its padding largely
repeats that value. This recipe cannot reproduce the tested spatial patches at
that resolution. It explains the enlargement; it does not prove every possible
alternative authoring technique impossible. These probes were not cooked or
installed.

UE 5.6.1 imports and cooks the textures in a private project. The first cook
omitted the commandlet's required Windows destination argument and emitted
nothing; the corrected cook emitted 13 packages. Freshly cooked textures replace
only their matching assets in the extracted baseline. The remaining 183 assets,
including the mesh and preserved compiled materials, keep their payloads.
The final IoStore validates; all 196 export identities and 270 non-header
payloads pass round-trip equality. Earlier extraction attempts lacked the shared
rig dependencies and were rejected before packaging.

`cw-package-03/decoded-audit.json` records DXT1 drift on occupied UV texels.
Per-texture roughness MAE ranges from 0.348 to 1.783/255; metallic MAE ranges
from 0 to 0.797/255. Roughness p99 reaches 13/255 and isolated maxima reach
166/255; metallic isolated maxima reach 239/255. Clean surfaces and palette
captures were reviewed without a visible new broad roughness/metallic defect
in this scene. This is not a pixel-identical cook. Cooked red >=128 coverage ranges from 23.9% to
65.7% of occupied texels. This is texture-space coverage, not screen-space
blood acceptance.

The original CW skin inherited an NPC material with
`USE BLOOD MASK=false`. All three shipped ShellKeeper body variants also
explicitly disable blood. The garment-only candidate does not satisfy the
requested skin blood ladder. Do not edit a static-switch property in cooked
bytes or claim a runtime scalar enables that shader branch.

The subsequent `cw-skin-parent-02` probe replaces only leg slot 4 with a dynamic
instance of the shipped `MI_Bone_Snail_Body` blood-enabled SSS permutation and
copies CW's resolved uniform parameters. Its clean appearance is visually close
to the original; the dedicated spatial BRM produces localized blood patches.
The original material, customization and saved state restore, with mask 1.0.
Both parents use `SSP_SkinV5` and the same base-property values. Their additional
static differences are Fresnel-mask and opacity-dither choices, so the probe
does not establish shader equivalence across every view or control.
`cw-authored-skin-01.json` resolves parameters from the authored constant assets,
not the user's dyed live instances. Nine parameter-only skin children and a
dedicated BRM were cooked in `cw-skin-cook-01`; the original shared skin
BRM remains untouched for the eye-socket graph that uses its red channel as AO.

The skin cook initially hit the existing `GShaderCompilerStats` null-pointer
failure under `-NoShaderCompile`. The established private `CWCookMaterials`
commandlet supplies the missing statistics object without enabling shader
compilation. Reusing it succeeds. This was an isolated editor-process failure,
not a game crash; exact commands and both logs remain in `cw-skin-cook-01/`.

`cw-package-04` combines the garment correction with nine parameter-only skin
instances and the separate `T_BRM_SkinBlood`. Readback verifies every selected
scalar/vector/texture parameter against the authored source, the intended
shipped parent, and absence of new static permutations or compiled resources.
Skin source green/blue arrays are byte-identical. Cooked roughness and metallic
drift are both zero, including maxima; occupied-UV red coverage at >=128 is
33.51%. All 197 export identities and 272 non-header payloads pass container
round-trip checks. The 174 untouched assets retain all 410 original file hashes.
Meshes, rigs, animations, eye materials and their shared BRM remain unchanged.

The combined candidate and revised Shared Assets metadata pass the native
catalog parser with both CW 0.1.0 and CW 0.1.1 (`catalog-validation-02`). They
were installed under the user's candidate-test authorization, with rollback
copies in `install-02/backup`. The first Steam launch request did not start a
process; an explicit normal launch succeeded. `install-02/after-resume.json`
verifies the cold start, original appearance, saved state and approved location.
No core DLL or Eve file was staged.

`cw-cooked-after-01` tests the installed garment candidate at all four blood
strengths. All 70 material identities and frozen body poses remain stable.
The capture's half-second clear delay was too short to reach exactly 1.0 in
every intermediate reading. The capture script now polls for a clean mask
before recording the cleared surface. The final acceptance batches below use
that check and reach 1.0 after every step.

`cw-acceptance-01` records six frozen-pose ladders: front and rear with the
full outfit, coat hidden, and exposed skin. `cw-acceptance-wide-01` adds four
full-body ladders with both boots/feet in frame. All 40 strength steps retain
the same body pose and all 70 material identities, and clear to native mask
1.0. Garments and exposed skin show localized patches rather than uniform
coverage. Front/rear views cover boots, coat, skirt/belt, leotard, top, gloves,
ruffles, lace and head accessories. Tiny or occluded details are additionally
covered by occupied-UV mask readback, not separate macro photographs.

The 24 color cases cover Original and all five named palettes, then all nine
production swatches for each displayed color control. The checker reads the
production swatch function rather than inventing colors. The 216 skin and 72
garment BRM binding checks all retain the installed blood textures. Captures
show the requested fabric colors. `cw-profile-02` saves a unique temporary CW
profile, changes the look, reloads that profile, verifies exact customization,
deletes its own profile and restores the original saved state. All four
pre-existing profiles remain unchanged.

The task explicitly named stock Genessa as a native reference. The user later
clarified that our Unholy Genessa should be used. `unholy-controls-01` adds both
`eins0fx.unholygenessa/genessa` and `/eve`, with four native strengths, stable
body poses/material identities and clear value 1.0. This changes the comparison
coverage, not the repair owner. Neither stock nor Unholy assets were modified.
Both variant memories, the outfit-wide remembered value and CW restore exactly.

A combined diagnostic material-swap attempt (`cw-paired-02`) let original MIDs
be garbage-collected, so CSS correctly refused subsequent customization due
to retained-material mismatch. That attempt and `cw-profile-01` are rejected.
A normal quit/relaunch removed the transient overrides. The closed-game state
matched the saved backup; `recovery-resume-01/after-resume.json` verifies the
original appearance, scene, installed hashes and exact saved settings. Profile
and control tests cited above were rerun after recovery. This was a diagnostic
failure, not a change shipped in the candidate. Subsequent pairing keeps the
original garment MIDs and changes only their texture bindings.

`cw-paired-03` supplies full-outfit front/rear and coat-hidden front pairs;
`cw-paired-05` supplies the coat-hidden rear pair. Each has clean and strength
0.5 captures with baseline garment textures reconstructed on the same MIDs,
then the installed cooked textures restored. Each pair verifies body pose and
camera equality. The old inputs produce broad black coverage; the installed
inputs produce patches, with similar clean appearance in this scene. Skin
stays on the candidate for both sides; its parent comparison is the separate
`cw-skin-parent-02` test. Run 03 stopped before its fourth pair when an imported
transient texture expired; restoration succeeded. Run 04 received SIGTERM before
its first pair and restored successfully. Run 05 completes that missing pair
with a bounded re-import retry for expired temporary texture handles.

`cmake --build build/host -j 4` and the existing host suite pass, 28/28 tests.
`catalog-validation-02` loads the combined CW/shared pair with the native catalog
parser and checks the original CW with the same shared metadata. Both scenarios
pass, including facial revision matching. No Windows core build is needed for
this asset-only candidate.

## Shipping decisions

CW's white garment mask is an asset defect and belongs in **Commander White
0.1.1**. No runtime blood change is justified. **Shared Assets 1.0.1** needs
metadata additions for the new CW container hash and bulk size: two performance
adapters and one facial adapter. Original revision entries remain, so both old
and new CW containers resolve. Shared cooked payloads are unchanged. The outfit
still declares `css.locomotion` / `eins0fx.shared.author_namespace.v1` unchanged.

The measured Peony world symptom belongs to the native weapon shadow path.
The inspected skin assets and palette bindings do not justify **Eve 1.5.1**.
No CSS or Eve skin change is prepared. `peony-preview-controls-01` shows the
same harsh body shading, surface noise and thigh shadow in Skin Suit, Black
Pearl and Peony. All three use the same CSS preview setup; the original look
and saved state restore exactly. Together with the shadow controls and exact
restored-skin identities, this supports a lighting/shadow cause in both contexts,
not a Peony-specific skin asset defect. CSS remains the user-approved installed
**1.0.1** baseline; no 1.0.2 work is opened.

Exact staged files are listed in `candidate-files-02.json`. Paths below are
relative to the evidence directory; game destinations use the same package
directory and filename under `MortalShell2/Content/Paks/~mods/`.

| File | SHA-256 |
| --- | --- |
| `cw-package-04/CSS_CommanderWhite_eins0fx_P/CSS_CommanderWhite_eins0fx_P.pak` | `3a0ae1445621d40ed22f2ce75c01293939c8db07e82f8664750a69cff6781b0e` |
| `cw-package-04/CSS_CommanderWhite_eins0fx_P/CSS_CommanderWhite_eins0fx_P.utoc` | `778c0a245773e85abace7ced433c8d13a622c2ea4176c337f7c2a451e7b6a8b6` |
| `cw-package-04/CSS_CommanderWhite_eins0fx_P/CSS_CommanderWhite_eins0fx_P.ucas` | `5debf8af8d3385096ee0e4f8c95dca4c6c39d69f026c0812aba1f0a94e127e01` |
| `shared-metadata-02/CSS_SharedAssets_P/CSS_SharedAssets_P.pak` | `15f0769619122b7dbaaf1c3a17a7226c564249c0247dc9310b0559d1d4a8d9a7` |

Shared `.utoc`/`.ucas` are unchanged. Their SHA-256 values are
`fde7763d895231785c611144a85a64146884431fa09264ce70f2f6d690fdd830` and
`135f8089780fc549fe34b776f158f7ee3d06e4219746f69ef93fdabd2bcbdaeb`.
Only the shared metadata `.pak` needed replacing. `install-01/backup` retains
the original baseline and `install-02/backup` retains the earlier garment-only
candidate. The latter install receipt records the combined candidate.

Separate local ZIPs, checksum files, receipts and changelogs are prepared:

- `dist/commanderwhite-v0.1.1-blood-candidate/CSS_CommanderWhite_eins0fx_P-v0.1.1.zip`,
  SHA-256 `8cba09461f8a23c64cde9c69c941b2289e10b7e7f4f8c21319ed768eb339343a`.
- `dist/shared-assets-v1.0.1-blood-candidate/CSS_SharedAssets_P-v1.0.1.zip`,
  SHA-256 `3a2bbdc9ad99695e9e92fc1f60dda568ab7db8907e0c0183bac5bb966e9fe1d5`.

The CW ZIP passes the existing package snapshot, verify and ZIP-readback
workflow. Both ZIPs contain the exact validated trio, with member hashes
verified. No public upload or tag was made. Frozen archives are untouched.
`final-file-verification.json` checks six installed candidate files, eight
unchanged baseline files, both Unholy cooked files against the frozen 0.3.0
release, and five archive checksums. Every comparison passes.
`final-live-verification.json` verifies CW restored at the approved location,
exact saved-state equality, mask 1.0, animation running, look input enabled,
no recovery pending and no reported maintenance/overlay error.

## Validation limits

The Peony production control resolver and save round-trip pass all 65,536
combinations (8,192 clothing patterns times four palettes times two glow levels),
including exact foot/stocking section restoration. Every individual removal,
all-off/all-shown, re-show, and all-off palette/glow cases were rendered in world
and CSS preview. The entire cross-product was not GPU-rendered. Thus the task's
literal exhaustive visual acceptance is not claimed. There is no changed Peony
asset requiring an invented after image.

Initial baseline stills have idle-pose variation. Controlled red-channel and
skin-parent pairs hold body pose and camera; weapon animation remains independent.
Diagnostic reconstructed baseline inputs must not be labeled screenshots of
two separately installed releases. Changes to visibility can restart deformation,
so separate clothing cases are not all identical-pose pairs.

Three garment masks preserve constant green/blue values while changing texture
dimensions, as detailed above. DXT1 introduces the measured cooked channel drift.
The new skin parent also differs in Fresnel-mask and opacity-dither static
choices. Resolved CW uniforms, clean appearance, displayed colors, blood and
clearing were checked in the approved scene; exhaustive lighting, translucency
and combat coverage is not established by that scene.

These are tested local candidates with explicit limits, not a claim that every
literal acceptance item or every possible view has passed. Existing saved
profiles remain byte-for-byte equivalent as JSON; the temporary CW profile
exercises loading the new package without changing those profiles.

The earlier candidate review was followed by the user identifying the dark layer
in their Peony screenshot as stockings, accepting that appearance, and explicitly
requesting all skin releases and BBCode changelogs under `dist`. That later release
instruction authorizes the local delivery. The measured matrix above remains the
record of what was tested.

## Completion audit

The last goal turn made progress by adding both Unholy control ladders,
finishing the garment diagnostic pairs, verifying restored state and installed
hashes, and committing the report and changelogs. The subsequent offline
original-size bake adds evidence for the remaining texture-size decision.
No game process or capture job is currently being waited on.

| Explicit requirement | Current evidence and disposition |
| --- | --- |
| Released baseline before changes | Frozen CW/Eve cooked payload matches; captures predate installation. CSS 1.0.1 baseline explicitly approved by the user. Met with that approved baseline change. |
| Decode masks and test hypotheses in order | 23 BRM histograms, static-switch inventory, dye binding audit and same-MID red-only comparison identify the asset cause. No live primitive-array claim is made. Met. |
| Native four-strength CW ladder, stock control and Black Pearl | Initial baseline and ten final CW ladders, plus final stock/BP ladders. Both Unholy variants added at the user's request. Captured views and clear values pass; tiny garment details are supported by UV readback rather than isolated close-ups. |
| Patchy skin as well as garments | Dedicated skin BRM, nine parameter-only child MIs, parent/parameter readback and exposed-skin front/rear ladders. Met in the tested scene; broader shader behavior is untested. |
| All displayed CW colors/palettes and saved profiles | Six palette states, 18 displayed swatches, stable BRM bindings and temporary-profile reload. Existing profiles are preserved; no existing CW profile was present to reload. |
| Peony per-piece preview/world matrix and four palettes/glow | All 13 single removals, all-off/all-shown, re-show, eight all-off palette/glow states and accepted skin controls captured. Met for those cases. |
| Every Peony combination visually checked in both contexts | Production visibility and save/load pass 65,536 cases. Full GPU cross-product and a screenshot for every combination are missing. Not complete under literal acceptance. |
| Exact restored face identities and accepted skin preserved | 185,442 equivalent skin corners, 4,944 covered triangles, exact material/UV/tangent/normal identities; unchanged Eve package hashes. Met. |
| Matched before/after stills for every matrix case | Controlled red/skin-parent and garment pairs match body pose/camera. Initial baseline and separate visibility cases do not all match; weapons animate independently. Not complete under literal acceptance. |
| Byte-identical source roughness/metallic and measured DXT1 drift | Exact arrays for ten garment masks and the skin mask; constant values preserved across enlarged dimensions for three garment masks. Drift quantified. The three dimension changes remain a literal deviation. |
| Rebuild through existing recipes/readback, no cooked byte edits | MeshExport, Blender bake, UE commandlet cook, retoc container round-trip and native catalog checks. Met. |
| Host tests and conditional Windows build | 28/28 host suites pass. Runtime unchanged, so a Windows core build is not applicable. |
| Assign owner and preserve contracts/frozen releases | CW 0.1.1 owns assets; Shared 1.0.1 owns revision metadata; no Eve/CSS patch. Existing contract and old CW revision resolve. Frozen archive checksums pass. Met. |
| Separate artifacts/changelogs and exact staged-file report | Two verified local ZIPs, separate BBCode files, full staged paths/hashes and rollback receipts. Prepared as candidates, not published releases. |
| Approved scene/staging, restore and cleanup | User-approved scene and installs; final appearance/state/input/animation/blood checks pass. Temporary profile/effects/overrides and owned bridge request files cleared. Met. |
| Commit only own paths; no public main push | Investigation branch commits contain only the finding doc, index/queue entries and separate changelogs. No push. Met. |

The audit above records the original candidate checkpoint. The subsequent local
release request is fulfilled by Commander White 0.1.1 and Shared Assets 1.0.1,
with unchanged Eve 1.5.0 and Unholy Genessa 0.3.0 archives. Each has its own BBCode
changelog. See `dist/DELIVERY.md` and `dist/coordinated-v1.0.1.json` for final
paths and hashes. No public upload is part of that instruction.

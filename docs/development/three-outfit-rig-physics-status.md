# Three-outfit implementation status

Updated 2026-10-09. [Goal](three-outfit-rig-physics-goal.md) is active. The coordinated namespace/facial trial and world cloth correction are installed. The user accepted the Genessa facial-playback, visibility and world rear-panel correction; the full regression matrix remains pending. Earlier sections describing private candidates record their state before this deployment.

Latest installed core: `718f76c3b60a8315`, with the CW hair trial. The user
reports the requested CW Iris Out/hair/eyes/interruption check looks fine.
The runtime snapshot confirms `SK_Facial1`, available Iris Out/gait choices
and no catalog, animation or recovery errors. Individual actions were not
enumerated. CINE, doubles and matched performance remain separate checks.

## Coordinated upgrade instructions and current dependency check

The [draft upgrade guide](../upgrade-v1.0.0.md) now lists the runtime/outfit
destinations, replacement of the old shared trio and retirement of the separate
locomotion/Astral packs. It preserves state and profiles and requires a normal
restart. The package-author guide and CLI install refusal no longer recommend
core hot reload.

`work/three-outfit-rig-physics/dependency-pruned-cw1` passes 28 dependency and
upgrade scenarios using the current `hair-strand-pruned1/validation2` candidate
set. The legacy inputs come from the original namespace transaction's backup,
with hashes verified against its receipt, rather than the now-updated mounted
paths. The dependency test binary was rebuilt from the current source.
`candidate-receipt.json` pins all twelve candidate files, manifests and test
tools; cooked-container hashes and sizes match their metadata.

The complete candidate set resolves nine facial bindings. Missing, incompatible
and duplicate libraries produce the expected install, update and duplicate-copy
messages. The unchanged old set retains seven facial bindings, while mixing old
outfits with the new library does not acquire bindings for another revision.
This checks catalog behavior, not Unreal playback, installation or visual fit.
No game files were changed for this check.

The candidates still carry Eve `1.4.0`, Genessa `0.2.0`, CW `0.0.6-dev` and
shared `0.1.0-dev` metadata. The final coordinated version stamps, release ZIPs
and remaining gameplay/performance gates are pending. Do not label these private
trials as the requested release versions.

## Skin Suit editable-source comparison

The suit's saved Blender source matches its current import payload across
18,633 points, 33,352 triangles, named weights and six body morphs. The separate
2,385-point ankle lining source exposed a saved `FBMBodyTone=1.0` value: its
evaluated shape differed from the neutral import by 0.2998159 cm. Resetting that
value in memory removes the discrepancy within float precision. The original
source is preserved.

The lining-generation helper now explicitly saves neutral shape values and
checks them after reload. A fresh private lining library passes without an
override. Source and game payloads are unchanged. This closes two garment-part
source comparisons, not the whole outfit's source-to-cook provenance. See
[Eve's source note](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/authoring/skin-suit-source-provenance.md)
and `work/three-outfit-rig-physics/skin-source-provenance1`.

The subsequent `skin-cooked-provenance1` check covers the entire Skin Suit import
payload against the current cooked normal mesh: 114,373 source points, 444,427
render vertices, 153,507 triangles, 379 bones and 22 positional morphs. Position,
UV0, material keys and triangle multiplicity/winding agree; weights and morphs
are within the documented cooking bounds. Maximum influence difference is
0.019563 from the eight-bit representation, not byte-exact preservation. The
audited Eve trio matches the installed trio. Deliberate winding, weight and
morph changes are rejected. Blender body/hair provenance, other render fields,
the facial mesh and live fit remain separate. Nothing was installed.

The expanded source check in `skin-source-complete1` now also compares all
declared corrective morphs, three UV channels, corner normals and vertex colors.
Five of the six parts pass, covering 111,988 points: body, suit, both hair meshes
and the hair sword. The body includes all sixteen left-hand corrective morphs.
The lining passes geometry, weights, UVs and shapes, but its rebuilt Blender
source changes color values and loses five corner normals. The source-saving
helper now preserves the linear color bytes; a new `skin-source-linear2.blend`
verifies that repair. Its normals still fail, so it is not a complete source
replacement. The installed import and package remain unchanged. Deliberate
UV, normal, color and thumb-morph mutations are all detected in
`skin-source-negative1`.

The subsequent `skin-source-preserved3` repair closes the lining's export-normal
failure. The source library retains float corner normals, guarded by hashes of
its local geometry, corner order and normal values. Changed geometry, evaluated
shape, missing/wrong attributes and altered normals fail explicitly rather than
export stale data. All nine guard/fallback cases pass. The rebuilt lining now
passes every field in the expanded source comparison; its maximum normal-vector
error is `1.713e-7`. The ordinary Eve body path was rechecked and still passes.
All six part sources now account for 114,373 points and 153,507 triangles in
Eve's `authoring/skin-suit-source-manifest.json`. This closes the listed
source-part correspondence, not the remaining whole-outfit rig, facial,
shading-graph, cook and gameplay gates. No installed files changed.

## Queued Eve outfits: source inspection

Both supplied outfit archives are extracted and pinned without changing the
originals. Peony contains two mesh variants; Slipstream contains four. All six
decode with identical geometry/weight readbacks in isolated and retail-assisted
mounts. They represent three distinct geometry/weight sets. Source material
chains and 87 explicitly referenced texture packages are exported; incomplete
automatic GLB material conversion was rejected as a visual baseline.

The reviewed section views identify the modularity work: several garments share
material regions, and Slipstream Normal lacks lower-leg body surfaces under its
boots. Removable components need deliberate separation and body restoration.
No new outfit candidate, CSS controls or palette templates are complete yet.
See [Eve's source inventory](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/authoring/new-outfits-source-inventory.md)
for counts, hashes, proposed component groups and remaining work. No installed
files changed. CW's moving foot-control comparison remains pending.

The next component audit accounts for every triangle in the three source
geometry sets. Slipstream's shared jacket/skirt material separates into two
disconnected surfaces (3,176 and 2,082 triangles), allowing independent garment
ownership without cutting either surface. Trim ownership, fit and controls
remain to be authored. The user subsequently confirmed CW belt physics and
skirt smoothing are queued for later, with Eve taking priority.

Eve now has an editable source library with 23 parts across the three geometry
sets. Fresh Blender reload accounts for all 249,020 triangles with their source
UVs, named weights, raw normals/tangents and color bytes. Maximum position
component error is `5.723e-6` cm. Slipstream's jacket/skirt split is preserved.
The offline MeshExport helper gained an optional ordered-surface export after
GLB face reordering was found; its ordinary skin-audit output is unchanged.
Three source comparisons, the unchanged default mode and invalid-mode rejection
pass. These remain source-sized parts with placeholder materials; body fitting,
target rig binding, morph transfer, trim ownership and gameplay are pending.
Two zero-length Peony source normals are retained and flagged for repair.

## Current shared-assets trial

The corrected `shared-cloth-native2` pack and matching core `645effe0a1d0249d`
are installed and independently hash-verified. The first pack loaded native
sequences but lacked 52 automatic idle carriers, causing `Unavailable`.
All 60 native Idle Animation choices now have carriers, alongside three gait
choices. The user confirms Black Hole and Iris Out play and movement/attack
interrupts normally on the tested original human appearance. This does not
accept every native clip or CINE playback. All 24 host suites, 422 native catalog
checks and 1,386 outfit checks pass. The combined 855-package dependency check
passes. The three outfits, accepted cloth correction and saved settings retain
their hashes. This is not an upload-ready release. See
[installation and acceptance evidence](shared-animation-native-human.md#october-9-missing-native-idle-carriers-found-in-gameplay).

## Current Genessa hair trial

The Genessa-only graph change in outfit commit `4b49f08` disables an active
hair solver with no render, cloth or observed attachment consumer. Both normal
and facial graphs are covered. The shared metadata updates its three Genessa
revision bindings; shared cooked assets are unchanged. Fresh reload, cook,
336 paired component samples, 855 dependency checks, 422 native catalog checks,
1,386 outfit checks and metadata readback pass.

Installed with the game closed on October 9. Six package files were independently
hash-verified; the current core `645effe0a1d0249d`, Eve, Commander White, profiles
and CSSX/CINE files are preserved. Receipt and exact hashes:
`CSS_UnholyGenessa_eins0fx_P/work/hair-solver-pack3/installed.json` in the outfit
authoring repository. Rollback is outside the mount tree at
`MortalShell2/Content/css-genessa-inactive-hair-trial-20261009` and uses the
existing namespace installer's `restore --transaction` command.

The user has been told this trial is ready to launch. Appearance, animation and
matched gameplay cost still need live verification. Removing unused simulation
does not prove a particular FPS improvement. See the
[solver evidence](../research/2026-10-08-secondary-motion-ownership.md#october-9-hair-lifecycle-and-genessas-unused-solver).

## Current Commander White hair prototype

The original binding combines a long right strand and a separate short strand
on one chain. The latest private trial restores two short-strand joints and
redistributes only their existing hair weights. The prototype passes 1,446
head-turn/gait samples, preserves the other six chains exactly, and clears the
previously confirmed right-side flyaway regression. The original matched
source-opacity witnesses also clear; the wider 21-frame screen finds no
section/frame worse than production by that diagnostic. Three source-material
renders are reviewed.

The next private candidate now exists as saved UE assets in CW's
`work/hair-strand-assets1`: a CW-only skeleton with 388 raw bones and nine
rebuilt virtual bones, both normal/facial meshes, and the extended hair rig.
Both meshes retain geometry, material sections, cloth and their 16/206 morphs;
936 source vertices per mesh receive the separate short-strand weights.
Fresh reload preserves all 395 existing rig hierarchy entries, the original
array prefixes and control defaults. Both post-process graphs pass a structural
comparison permitting only the new skeleton/hair class, two extra hair output
bones and one hidden contact input. The rebuilt mesh completes 723 compressed
walk/jog/sprint samples without the hair solve changing non-hair bones.

The rebuilt-mesh check also passes all 60 fixed contact witnesses over sampled
gait poses. Maximum segment-length error is 0.0000048261 cm; other chains are
numerically equivalent within 1e-10, not bitwise identical after skeleton
remapping. Rendered quantization differs from the JSON prototype only in the two
edited sections, including 87 head influences changing by one 8-bit step.
Source non-target influences and other inspected sections retain their values.
CW commit `9a92d08` records the recipes and evidence; six private asset hashes
are in `hair-strand-assets1/candidate-hashes.json`.

CW commit `0c99728` adds six full-component original/candidate pairs covering
both meshes and three gaits. All 2,892 samples pass hair toggles, reset epochs,
animation reinitialization, teleport and selected morph application/clear.
Body state and non-hair transforms match within the stated numerical tolerance.
Cloth is disabled only in this transient editor diagnostic, not in the assets.

The Windows cook and six-package IoStore roundtrip now pass. Independent
readback against installed C4/E9 preserves all material slots, 16/206 morph
delta digests, cloth exports/mapping, original binds, sockets and virtual
definitions. All cooked positions and triangle indices match. The 2,548
changed hair vertices match the saved editor weights after explicit 8-bit to
16-bit unit conversion; weights outside the two hair sections are unchanged.
`hair-strand-pack1/semantic-check.json` records its precise coverage. Only the
six new mesh/rig roots are packaged. Cooked dependency materials, including
the optical material with shader warnings, are excluded in favor of installed
materials. Completed cook caches and Wine prefix were removed with a receipt.

That stage established offline evidence; the later full trial is installed
as recorded below. The two left-bang head-turn samples now have source-opacity
comparisons: both improve over the original but retain small intersections.
Astral/CINE runtime integration, full gameplay lifecycle and cost remain pending.
The shared skeleton remains unchanged. Recipes, limits and rejected harness evidence are in
the [CW hair study](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/hair-head-clearance-study.md#original-strand-binding-and-separate-joint-prototype).

The final namespace candidate is now `work/hair-strand-final3` in the CW repo.
All eight assets save and reload in a fresh editor with old redirectors removed:
the six mesh/rig assets plus distinct normal/facial CINE playback graphs. The
two scene graphs each pass an exact reflected comparison with 82 bijective
duplication-GUID mappings and only the five planned hair/skeleton changes.
Separate scene classes preserve the existing one-to-one playback restoration
lookup; the runtime's ambiguity checks are not relaxed.

`hair-strand-assets1/playback-component1` passes 2,892 editor component samples
through scene enable/cancel, hair toggles, reset, reinitialization, teleport and
selected morph changes. A separate comparison checks six native leg bones in
all twelve captures: scene playback changes the pose during frames 30-199 and
every inactive frame returns to the input gait within 1e-8. This does not test
CINE's runtime input handling or synchronized facial curves. The final-path
Windows cook and eight-package roundtrip now pass. Independent readback in
`hair-strand-final-pack1/semantic-check.json` preserves each mesh's 70 material
slots, 16/206 morphs, cloth data and original bind prefix. Separate surface
digests also prove cooked positions, normals and UVs unchanged; only the
previously audited 2,548 hair vertices change weights. Both scene graphs retain
their playback defaults and extended hair bindings. This completes the final
cooked-data comparison, not gameplay or cooked graph execution.

The CSS candidate accepts shared base animations on the exact audited CW hair
skeleton, preserving exact-target preference and legacy fallback. Reverse and
cross-extension animation remaps remain rejected. All 24 host suites and the
Windows core build pass. Staged core SHA-256:
`718f76c3b60a8315f8ba4fb9e9f9c11872d8575d6c64aada3c9774a105dec590`.
Receipts are in `work/three-outfit-rig-physics/cw-hair-integration1`.
The core and CW candidate are now installed, as recorded below. The rejected `hair-strand-final1`
copied Content/Binaries were removed after confirming the failed process was
terminal; its recipe/logs remain. Logical deletion was 1,336,311,290 bytes,
with zero measured free-space increase because of shared reflink blocks.

The complete trial is assembled in CW `work/hair-strand-assembly1`. All original
CW export/bulk payloads roundtrip unchanged alongside the eight new roots.
Shared cooked payloads and facial curve resources remain unchanged; its catalog
pins the new CW revision and binds the new normal/facial meshes and separate
scene graphs. `validation2` passes 864 package checks, 228 catalog references,
426 native-library checks and 1,396 nine-variant checks, including both scene
restoration paths. The first validator attempt incorrectly included 33 legacy
alias identifiers as load requests; its failed receipt remains. Aliases are
still tested through actual catalog resolution, not required as mounted files.

Installed with the game closed on October 9 through the existing transactional
installer. Six package files and core `718f76c3b60a8315` were independently
hash-verified. Eve, Genessa, saved settings and CSSX/CINE files retain their
hashes. All 24 host suites and 22 installer tests pass. CW's `installed.json`
records the exact files. Rollback is at
`MortalShell2/Content/css-cw-hair-trial-20261009`, outside Paks, using the existing
namespace installer's `restore --transaction` action. The user reports the
requested CW Iris Out/hair/eyes/interruption test looks fine. The read-only
snapshot in CW `work/hair-strand-assembly1/live-check1` confirms the new facial
mesh and available animation choices without catalog/animation/recovery errors.
This is visual acceptance, not an exhaustive action matrix. CINE/Astral runtime
checks and matched performance are still pending. Original
CW mesh/graph payloads remain for this trial; retire unused ones only after a
reverse-reference audit before release.

The separate CW `hair-strand-pruned1` candidate now excludes eight superseded
mesh/graph/rig assets. The reverse-reference audit covers imports and `/Game/`
name-map paths across all 864 packages, active catalogs and 243 runtime source
files; retained consumers do not reference those roots. The reduced candidate
passes 856 package checks, 228 catalog references, 426 native checks and 1,396
nine-variant checks. All 413 retained payload files roundtrip unchanged.
Package size drops by 395,414,601 bytes; no installed file changed. The
[CW study](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/hair-head-clearance-study.md#superseded-asset-cleanup-candidate)
records exact artifacts, limitations and intermediate cleanup. This finishes
the current offline cleanup step; the user's walking-foot report is next.

## Current form-switch regressions

The Customize-only-Ground-Height defect came from inconsistent appearance
selection: rendering used the mirrored living Genessa outfit while menu and
facial consumers read the independent, stale Stray selection. Commit `b1e1cbe`
uses the existing mirrored-selection resolver consistently. All 24 host suites,
153 animation checks and the Windows build pass. The core
`css_core-form-selection-48690559fd075516.dll` is installed and hash-verified;
packages and saved settings were preserved. After relaunch, the user confirmed
the first Faithful-to-Stray summon works and all Customize controls remain.

The separately reported first Stray summon failure did not recur in that test.
Read-only capture confirms an active Stray ability, primary clone and CSS
Astral visual after the first logged transition. Its original cause remains
unproven; no speculative notify/spawn repair was added. The capture stopped
normally. See
[the investigation](genessa-form-selection-regression.md).

## Live trial correction: facial playback and component visibility

Release targets specified by the user: Eve v1.5.0, Unholy Genessa v0.3.0,
Commander White v0.1.0, plus **CSS v1.0.0** and a separate consolidated
shared-asset ZIP. The user chose the stable CSS milestone on October 8; this
does not change the outstanding release gates. Inspection of the actual distribution folders finds none of these new
outfit release versions packaged yet. The installed coordinated trial and
accepted facial/action tests do not constitute upload-ready releases. The
native library now has all 59 newly authored alternatives in a merged shared-pack
candidate, preserving the original four native gaits. Fresh authored-key
readback passes all 59. Nominal-time compressed comparison passes 58; Cinderella's
0.0198305 cm virtual-hand difference is traced to ACL float-time sampling.
Its complete 8,381-frame matched-clock comparison passes the unchanged 0.01 cm
bound (maximum 0.00656854 cm). No motion or codec edit was needed. The rejected
C12 precision trial is not selected. Seven new native idle carriers pass fresh
reload and 1,092 interpolation queries. All 67 cooked inputs pass game-only
container readback. The merged `shared-native-library1` contains 215 packages
and passes 416 native catalog checks both alone and with the outfits, plus the
existing 1,380 outfit catalog checks. It is not installed. Contact, performance,
gameplay regression and release packaging remain open. See
[native conversion evidence](shared-animation-native-human.md#october-8-conversion-batch).

The first namespace trial loaded Genessa's new facial mesh, but the player retained
Eve's `ABP_Eve` performance override after changing outfits. Genessa's authored
graph is `ABP_Wings2`; its facial adapter is `ABP_Genessa`. The retained Eve graph
prevented the verified Genessa playback binding. The menu logged `Preview body
geometry class differs from player`, which aborted synchronization before hidden
material sections were updated.

The runtime now detaches its own deferred performance override before a CSS mesh
change. A game-driven mesh change also retains cleanup responsibility until the
native action permits restoration, without copying the previous outfit's rig
inputs. Preview section visibility is synchronized before physics. Different
preview/player graph classes are accepted only when their parsed body geometry
models agree, allowing the normal graph and its performance adapter to coexist.

Windows compilation, all 23 host suites, and 14,364 geometry checks pass. The
corrected core `css_core-face-visibility-d3c00bddd78e4ef0.dll` is installed and
hash-verified. Outfit containers, profiles and saved settings were not replaced.
The user confirmed Genessa Iris Out facial motion and Shown/Hidden controls are
fixed after relaunch. This accepts those two reported defects on the tested
appearance. The user subsequently confirmed switching between Eve, Commander
White and Unholy Genessa with Iris Out selected: faces and eyes stay correct,
and attacking or aiming interrupts playback normally. This passes that live
cross-character test. Exact outfit/body variants were not specified. Both
Genessa body variants, CINE playback, form changes and doubles still need their
own candidate regression results.

Evidence: `work/three-outfit-rig-physics/live-face-visibility1`. The reusable
read-only `tools/diagnostics/check_live_animation_owner.py` compares the live
graph with the mesh's authored graph and catalog adapters. Its first automated
retry timed out because the game had closed; earlier reflected captures contain
the mismatched graph and disabled facial playback. This is not a completed live
regression pass.

Rollback order: with the game closed, restore the `previous_selector` from
`live-face-visibility1/validation.json` before invoking the original coordinated
namespace transaction restore. That transaction intentionally rejects a later
core selector rather than silently overwriting it.

## Weighted-surface coverage

### CSS binding for the cloth correction: installed trial, live check pending

The runtime candidate binds body-shape weights to opted-in graph variables named
`CSS_Morph_<morph name>` on the player, preview, worn items and Astral visuals.
Weak instance tracking replays weights after deformer replacement; unchanged or
unsupported variables are cached. The runtime leaves graph selection to the
authored mesh and preserves component overrides. See the
[binding contract](cloth-morph-deformer-binding.md).

The C++23 Windows core builds without new warnings and all 24 registered host
tests pass. Its SHA-256 is
`54bee458fb7cb1508ee8422db59d2ad45aba0f2778d6a132fe0b29dc27a30467`.
These checks do not establish in-game correction or performance. The isolated
Study5 graph/BasisStudy3 meshes now pass authoring and fresh-editor reload with
matching variable names and a mesh-authored default deformer. Earlier Study4
assets and the accepted installed build remain unchanged.

`genessa-deformer-render1/basis-runtime1` records both variants' unchanged rig,
137 morphs, 26 sections and cloth data; the author/reload commands exit 0 and
all pre-existing source assets keep their hashes. Module build18 exits 0.
`genessa-deformer-cook4` reuses the verified Windows single-thread cook recipe;
it exits 0 with unchanged inputs and successful SM5/SM6 kernel compilation.
`genessa-deformer-pack3` passes six-package export roundtrip and independent
readback: both default graph references, target collections, fifteen attribute
buffers, five zero-default GPU variables and kernel-source equivalence pass.
The independent mesh comparison rejects changes outside the stated additions.

The October 9 follow-up covers all four production mesh paths, including the
regular 22-morph meshes and the 137-morph facial replacements. Isolated authoring
and fresh reload pass with the shared graph renamed to
`/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphBody` and its matching collection.
The normal-mesh authoring wrapper initially caught an unnecessary resave of the
private Study5 graph. Its accepted bytes were restored from the verified cook,
and the recipe now saves the graph only when it creates it. Original inputs
remain unchanged. The final Windows cook exits 0 with unchanged inputs.

`genessa-cloth-assembly1` contains the complete Genessa and shared packages.
All 737 package import checks and 1,380 catalog checks pass. Independent cooked
readback of all four meshes matches the installed baseline outside the declared
attribute/default-graph additions and associated buffer metadata. Skinning,
materials, existing morphs and cloth data remain unchanged. The canonical graph
retains the tested kernel sources, five zero-default GPU weights and fifteen
attributes; both kernels compile for SM5 and SM6. A deliberately incorrect
variable is rejected by the verifier.

The follow-up installer stages only Genessa, shared assets and the CSS core.
Eve and Commander White remain pinned to the accepted installed files. Their
candidate metadata contents match the baseline; repacking Eve changed archive
bytes, so the installation retains the original archive. This exact package
combination passes the 1,380 catalog checks. Eighteen installer tests cover
initial migration, partial follow-up rollback, retained-file changes and guards.
`genessa-cloth-assembly1/preinstall-validation.json` records these results.

The follow-up was installed October 9 with the game process absent. All six
replaced package files and the selected core were independently hash-verified.
Eve/CW packages, saved CSS settings, CSSX/CINE files, loader and previous core
remain unchanged. The selector now points to
`css_core-cloth-morph-54bee458fb7cb150.dll`. The transaction and previous packages
are outside Paks at `MortalShell2/Content/css-cloth-morph-trial-20261009`.
`genessa-cloth-assembly1/installed.json` records the installed hashes.

With the game closed, rollback is:
`python3 tools/asset-paths/install_namespace_trial.py restore --transaction
/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Content/css-cloth-morph-trial-20261009`.
It restores the previous selector and two packages, without overwriting profiles.

On October 9 the user confirmed the world rear-panel fit works and supplied
`screenshot-2026-10-09_00-39-27.png`. The menu screenshot at `00-39-12` still
shows clipping at Glute Size 1. The user accepts that menu-only limitation for
now because gameplay appearance is the priority. This is world-fit acceptance
on the current appearance, not preview/world parity or a full movement pass.
`genessa-cloth-assembly1/user-world-acceptance.json` records the statement,
screenshot hashes, installed core identity and current mesh; the contemporaneous
status snapshot is retained alongside it. Keep the working world correction.

Both variants, continuous contact, doubles, lifecycle recovery and measured cost
remain pending. Menu clipping is recorded as accepted for now, not fixed.

### October 8: reproduced Genessa rear-panel shape mismatch

The user confirmed that the current Genessa rear panels cover correctly in the
world at Glute Size 0, but not at 1. Read-only matched captures found the same
mesh, saved morph values and enabled skirt section in preview/world; cloth is
disabled only in preview. Body-rig inputs also differ and remain a separate
unresolved observation. `genessa-panel-world1` preserves the requests/results.

Fresh UE 5.6.1 section readback of both facial meshes exits 0 with unchanged
source hashes. `genessa-panel-morph1` and the outfit's `panel-morph-transfer1/2`
identify direct render-morph suppression by cloth and test physical shape
transfer without modifying installed assets. The two variants produce identical
transfer arrays. The first linear fit leaves a 0.7985 cm maximum glute residual
near the rear center, so it is not a completed repair. Preserve the previous
rejected pinning and unsafe shared-mutation experiments. See
[the current outfit study](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/authoring/rear-panel-morph-notes.md).

Full cloth fitting, safe runtime ownership, matched motion/clearance and
performance remain open. Trebuchaxe visibility is still queued, uninvestigated
and not fixed. No current study changes have been installed.

The coincident-vertex check found no conflicting morph deltas above 0.1 cm
within positions rounded to 0.001 cm, so a welded seam is not established as
the transfer failure. Live read-only class discovery confirms OptimusCore and
ComputeFramework are loaded. Exact UE 5.6.1 graph inspection shows the stock
cloth blend discards the direct morph offset on fully simulated vertices.

`genessa-deformer-candidate1` privately duplicates the stock morph/cloth graph,
expands its functions and adds the skinned morph delta multiplied by cloth
weight after position blending. Its normal/tangent recomputation kernel is
unchanged. All protected engine graph hashes remain unchanged. C++20 module
build, graph compile/save, 108 CPU float32 position cases across both W3 facial
meshes, Windows SM5/SM6 shader compilation and the 673,422-byte cooked export
roundtrip pass. Evidence is in `genessa-deformer-study1`,
`genessa-deformer-cook1` and `genessa-deformer-pack1`.

These position cases use synthetic rotations and cloth motion. This is not
proof of rendered fit or collision response. A render offset can interact with
already enlarged colliders; physical clearance, normals/eyes/materials, zero
morph and disabled cloth, menu/world consistency, both variants, doubles and
GPU/runtime cost still need acceptance. The game remains on the existing
accepted build. No cloth assets, skeletons, gameplay inputs or settings were
changed by this prototype.

After the successful cook exited and its container was confirmed absent,
the disposable Wine prefix was removed (1,084,763,562 logical bytes).
`genessa-deformer-cook1/cleanup.json` records the deletion; cook inputs,
logs, shaders, cooked outputs and the verified trial package remain.

The offscreen Vulkan comparison in `genessa-deformer-render1` is complete.
Attempt 3 exits 0 with 24 captures, eight finite GPU readbacks of 162,939
vertices each, six active cloth surfaces and 3,420 simulation particles per
case. All 158 protected source hashes remain unchanged. Earlier attempts
stopped on a Python setter/property collision and a wrong instance-name
assertion; their logs and two preliminary captures are retained.

**Reject the first deformer candidate as a repair.** Both variants reproduce
the size-1 panel exposure. The stock and trial rear views each contain 17,831
body-colored pixels in the fixed diagnostic rectangle at size 1, versus 298
at size 0. These include intentional openings and are comparative evidence,
not a surface penetration metric. The expected retained skirt offset reaches
1.7993 cm; actual trial-minus-stock skirt displacement reaches only 0.00534 cm
on Eve and zero on Genessa. The slight differences are consistent with separate
cloth runs. See `gpu-comparison.json`; static math and cook success were not
proof that morph input reached the graph.

Exact UE 5.6.1 source explains the missing input: `SkeletalRenderGPUSkin.cpp`
sets `bSectionUsingMorph` false for cloth sections. `GetMorphVertexBuffer`
then returns null, and Optimus's morph interface returns zero for that section.
The next bounded prototype must supply the clothing morph basis independently
of that disabled input. Investigate cooked vertex attributes with per-instance
weights before making any runtime or installed-asset change. Keep simulation,
collider-fit and body/post-process synchronization checks separate.

Source inspection found an additional packaged-game requirement for this
approach: `OptimusDataInterfaceHalfEdge.cpp` can generate missing half-edge
buffers in editor, but rejects the deformer when cooked buffers and CPU
index/position data are both unavailable. The migrated mesh snapshot declares
no `TargetMeshDeformers`. An accepted trial therefore needs explicit mesh cook
dependency/buffer validation, not only a shared graph package. This is recorded
in `genessa-deformer-render1/packaging-requirement.json`.

### Independent cloth-basis prototype: rendered improvement, not installed

The next private graph, `DG_ClothMorphStudy3`, reads the skirt's glute offsets
from three scalar vertex attributes and a per-instance weight. Its additional
input is gated by `ENABLE_DEFORMER_CLOTH && !ENABLE_DEFORMER_MORPHTARGET`.
UE 5.6.1's cloth interface selects that permutation per render section from
the actual cloth buffers. This avoids adding the glute morph twice to ordinary
skinned sections, including vertices shared at material boundaries.

The boundary probe found 329 shared source vertices, 176 with nonzero glute
offsets (maximum 0.358853 cm). Display/imported skirt names were both
`UG_Skirt`; a name mismatch was not the cause. No vertices were split and no
engine code was changed. The prior ungated `Study2` graph was never rendered
as a candidate or installed.

`genessa-deformer-render1/basis-study2` records two private mesh copies with
unchanged render geometry, UVs, colors, skin weights, cloth mappings and full
cloth assets. Fresh-process `reload/result.json` extends the comparison to all
morphs and the skeleton on both meshes. Original source files remain unchanged.
The authoring and reload commands both exit 0. Build evidence is
`deformer-study-module2/exit12.json`.

The offscreen render command `basis-render-command1.json` exits 0 with all
24 captures and eight GPU readbacks. `gpu-basis-comparison.json` reports:

- Both variants restore up to 2.742668 cm of skirt displacement at Glute Size 1,
  compared with the 2.743002 cm source target. The maximum per-vertex difference
  from the expected offset is 0.014979 cm across separate cloth runs.
- Every vertex outside the skirt has zero trial-minus-stock position difference
  in the size-1 comparison, including the other cloth sections.
- Neutral skirt differences remain below 0.001264 cm. Rear diagnostic exposure
  changes from 17,831 body-colored pixels to 159 at size 1, versus 298 in both
  neutral stock/trial captures. These are fixed-region comparative pixels,
  including intentional openings, not a collision or penetration metric.

The rear render was visually inspected and shows restored panel coverage.
This is an isolated reference-pose **glute-only** result with full cloth
simulation, not the final fix. Other shape controls/combinations, suspended
cloth, animated poses, normals/materials, physical clearance and body-rig
synchronization still need validation. Expand the proven input approach as
required, declare the mesh's deformer collection, verify Windows cooked vertex
attributes/half-edge buffers, then test the packaged game and doubles. Runtime
weight binding and performance are not implemented or accepted by this study.
The installed game build and user settings remain unchanged.

### Five-shape correction and Windows cooked-data verification

`DG_ClothMorphStudy4` extends the independent input to glutes, hips, thighs,
waist and body tone. Both `_BasisStudy2` mesh copies contain 15 float vertex
attributes and declare `MDC_ClothMorphStudy4` for the graph's build requirements.
Source and fresh-reload checks in `basis-study3` preserve all 137 morphs,
26 sections, bind data and full clothing assets. The private editor helper
build15 succeeds. Original inputs remain unchanged.

`result-shapes2-readback.json` contains 88 captures and 44 GPU readbacks.
`gpu-shapes2-comparison.json` passes all 22 stock/trial pairs: neutral, five
individual shapes, half/full combinations, and three cloth-disabled cases per
variant. Maximum offset residual is 0.02251134 cm across separate cloth runs;
non-cloth positions remain identical. This is reference-pose shape-input
evidence, not moving collision acceptance. The first render attempt failed
because a test-driver property setter reinitialized the preview component;
the helper now sets the cloth flag directly after initialization.

Windows `genessa-deformer-cook2` crashed in the skeletal vertex attribute build.
Its stack and exact engine source suggest a concurrent build race, but that
cause is not proven. `genessa-deformer-cook3` uses the same asset inputs with
`-nothreading` and completes with exit 0, zero cook errors and unchanged input
hashes. This setting affects the offline cooker only. Both graph kernels
compile for SM5 and SM6.

`genessa-deformer-pack2` packages the two original meshes, two candidate meshes,
graph and collection. All six export payloads roundtrip unchanged. Independent
readback verifies 15 attributes of 162,939 float values each and the required
162,939 vertex/692,844 edge normal-reconstruction data per candidate. All source
render-morph entries match their attribute values within 0.000001 cm; additional
small source offsets remain below the mesh's recorded 0.015 cm morph threshold.
After allowing only these additions, the collection reference and relocation
of empty Nanite bulk metadata, all 152 exports per variant match the original.
The missing-attribute negative control is rejected. Graph readback verifies
five zero-default weights, 15 named inputs and both kernel sources. It does
not decode or execute the compiled compute shader maps.

The CUE4Parse attribute reader previously read only the stride and left the bulk
payload unread. A pinned local patch now consumes and bounds-checks that data.
Its alignment fixture fails before the patch and passes afterward; nine malformed
or truncated inputs are rejected. See `tools/AssetReadback/patches/README.md`.

No candidate is installed. Animated garment motion/contact, material normals,
body-rig synchronization, game runtime weight binding, preview/world/doubles
lifecycle and measured performance remain required before acceptance.

`cleanup-cloth-cook3.json` records removal of both terminal cooks' disposable
Wine prefixes and the superseded provisional readback. Successful cooked assets,
source inputs, logs, crash evidence and final readback remain. The removed files
total 2,370,243,733 logical bytes; observed free space increased by 399,605,760
bytes because shared/reflinked storage is not the same as logical file size.

### First isolated gait comparison

`genessa-deformer-render1/motion-render-exit1.json` exits 0. The motion driver
uses pinned migrated Eve idle/walk/jog/sprint sequences, both Genessa variants,
neutral/Glute Size 1 and stock/trial graphs. Each run advances three seconds
with full cloth and post-process disabled. All 64 final-pose images and 32 GPU
readbacks complete with protected inputs unchanged. The 16 matched cases have
identical sampled body poses, finite output and zero non-cloth position changes.
The inspected walking and Genessa idle/jog/sprint rear captures show restored
upper-panel coverage at Glute Size 1.

Do not accept full motion equivalence from this run. Separate neutral cloth
simulations differ by up to 9.044045 cm in the sprint comparison, despite
identical body poses; the source of that difference remains unisolated.
`motion1-validation.json` explicitly accepts only its sanity checks and marks
cloth equivalence unaccepted. Compare both graphs against one held simulation
state next, then check continuous material/contact behavior. The reference-pose
and Windows cooked-data results above remain valid. Nothing was installed.

### Held-state gait comparison: neutral discrepancy isolated

The follow-up reuses the same component and cloth output for both graphs.
`StepClothDeformerStudy` can now refresh rendering without advancing animation,
simulation or world time. It hashes each cloth's ordered particle positions,
normals, LOD and both transforms. The driver checks that this hash remains equal
before and after graph switching and readback; it does not disable cloth.

`held-render-exit2.json` exits 0 with 64 images and 32 GPU readbacks.
`held-motion2-validation.json` passes all 16 matched cases. All eight neutral
cases have exactly identical positions, normals and tangents across the full
mesh. All eight size-1 cases preserve non-cloth positions and produce maximum
displacements from 2.596409 to 2.741694 cm, within the original glute offset's
bound. This isolates the earlier neutral discrepancy to the separate simulation
runs; it is not a graph-induced neutral deformation. Do not repeat that earlier
comparison as a blocker.

The first held attempt stopped on a diagnostic naming assumption: a newly
created instance may end in `_Instance` without a numeric suffix. The private
weight setter and instance assertion now accept that valid name. Module builds
16 and 17 pass. The game runtime was not edited or installed by this change.

Next, integrate authored default-deformer activation and morph-weight binding
for world, preview and Astral copies, then validate actual materials, contact,
lifecycle and measured cost. The shipped game's `CXXHeaderDump/Engine.hpp`
contains `GetDefaultMeshDeformer`, `SetMeshDeformer`, `UnsetMeshDeformer` and
`GetMeshDeformerInstance`. `CXXHeaderDump/OptimusCore.hpp` declares
`SetFloatVariable(FName, double)`, so a reflected call must use a double value,
despite its name. Exact 5.6.1 `USkinnedMeshComponent::GetActiveMeshDeformers`
uses the mesh default unless the component explicitly overrides it. Preserve
that ownership and avoid replacing another system's override. These API findings
are integration evidence, not proof of live support for this private graph.

All 18 normal/facial meshes now have per-section nonzero bone influence records.
The 570-section check passes normalization, indices and source immutability;
an independent W3 full-array comparison matches all 25 sections. All nine
normal/facial region-summary pairs agree. Genessa's body helpers affect her
fitted trim and clothing, while her hair rig has no identified skin/cloth weight
consumer. Check execution/cost before removing that rig. This narrows subsequent
contact work; it is not a dynamic fit or performance pass.

See the [weighted-surface findings](../research/2026-10-08-secondary-motion-ownership.md#actual-weighted-surfaces)
and `skin-influences1`. Completed hand-test caches were pruned with a receipt,
removing 157,390,336 logical bytes while retaining source and evidence.

## Initial installed trial

The [cooked solver inventory](../research/2026-10-08-secondary-motion-ownership.md)
now accounts for nine variants, 18 normal/facial mesh paths, ten graphs and eight
Control Rig assets. Five adapter comparisons pass, including input identities and
defaults; fourteen negative/control tests cover the checker. All output-connected
pose branches preserve bone-writer order. No overlapping named body/hair bone
writers were found. The [hand source study](../research/2026-10-08-hand-target-ownership.md)
now resolves 115 transform writers to 19 left-hand bones, with propagation
contained in that set and all 16 corrective morph names present on active
Eve/Genessa meshes. Six checker tests pass. Cooked VM literal identity, live
graph scheduling, actual contact and runtime cost remain unverified.

The W3 hand skin now has a posed screen on both variants using 453 retained
native-output fixtures each. 452 poses preserve the four reference seam pairs;
one full-overlay running-attack input adds two local thumb crease crossings.
The deliberately uncalibrated control produces 1,102 pairs. The identical
normal/facial skin buffers rule out facial integration as a hand-geometry change.
This flags a small offline contact case for current-graph reproduction, not a
proven gameplay regression or acceptance of the whole hand/weapon interaction.
See the hand study for source hashes, images, exact scope and cleanup evidence.

The current migrated hand graph has now been executed directly in UE 5.6.1 on
all 453 nonsingular inputs. Valid finite outputs preserve translations, scales
and all non-finger rotations. Fresh-output skin replay reproduces the same one
thumb case, ruling out stale native-output fixtures. A subsequent full W3 editor
component replay passes 42 enable/disable/reinitialize samples across both body
variants. Corrective curves reach the render morph weights and clear on disable.
The skin replay reproduces the same small thumb crease case. Gameplay visibility
and severity remain unverified; no hand repair is justified from this alone.

`namespace-deployment1/validation.json` records the independent post-install
check. The selected core is `css_core-namespace-7d0e24cf60df400c.dll`, SHA256
`7d0e24cf60df400c562f44fa3195a5e201df69790c6cf2a665118405c2893908`.
The three matching outfit trios and consolidated `CSS_SharedAssets_P` match
`catalog-namespace3`. The separate old Astral and Locomotion trios are no longer
mounted. All six previous trios are retained outside `Content/Paks`, in
`MortalShell2/Content/CSS-namespace-trial1/backup`.

The game was closed throughout replacement and verification. Fifty-nine
protected files, including saved settings, the old core, loader and CINE files,
remain unchanged. The installer stages independent copies before moving any
old package and records the plan before replacement. Fourteen fixture tests
cover successful upgrade/restore, failures during each replacement phase,
concurrent launch, duplicate packs, changed files and insufficient space.
These tests do not prove game startup or rendering.

The user has been asked to check startup, the saved appearance, Genessa facial
playback/eyes and gameplay interruption. This is a migration and facial test
build, not completion of the remaining rig/contact/physics work.

Restore the complete previous set, with the game closed, using:

```bash
python3 tools/asset-paths/install_namespace_trial.py restore \
  --transaction /mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Content/CSS-namespace-trial1
```

Restoration verifies both sets before moving files and preserves settings
changed after installation. Do not restore one outfit or shared pack alone.
If the game opens during replacement, restoration waits for an explicit retry
after closure rather than moving potentially mounted files.

Cleanup before installation recovered 1,779,421,184 observed bytes. Forty
duplicate cooked/readback files now resolve to byte-identical retained stage
files. The obsolete Commander White material-only container was removed after
all its asset chunks matched the final full outfit. Original inputs, installed
baseline, final stages, recipes and reports were retained. See
`cleanup-coordinated-upgrade1.json`.

## Historical baseline before namespace migration

- Installed core selector points to `css_core-idle-handoff-db5207d4722ca6af.dll`; its full SHA256 matches the previous deployment receipt.
- User confirmed the current Iris Out gameplay-interruption test: attack, aim/fire, Genessa doubles and MISC Genessa Form work.
- All three installed outfit payload pairs match the historical audited manifests: Eve 1.4.0, Unholy Genessa 0.2.0 and Commander White's recorded `0.0.6-dev` package metadata. Version labels here come from the manifests, not inferred release names.
- Six installed containers inventoried: the three outfits plus legacy Shared, Astral and Locomotion. Export counts are respectively 331, 131, 197, 28, 30 and 103.
- There are 28 overlapping export paths among these containers. All nineteen differing movement animations were decoded independently: the legacy copies use indices 379-387 for the final nine tracks, while the installed skeleton has 386 raw bones. Eve's copies use 386-394. All other decoded fields agree; this does not establish compressed byte identity. The skeleton exports themselves agree in indexed hash across their four providers. Do not let consolidation reintroduce the stale track maps.
- Installed locomotion metadata contains 63 entries: eight idles, seven dances, three catwalks, eleven entries internally categorized as performance, thirty-one photo poses and three gait entries. These internal categories do not authorize a new CSS Performances menu. Four entries declare native-human assets and **59 have no native-human asset declaration**. This is catalog coverage, not live validation. Two facial rigs are advertised, Eve and Commander White; no Unholy Genessa facial rig is present.
- Existing source evidence identifies the visible Genessa mask and eyes as separate meshes without expression shapes. Retained Eve body shape names do not establish visible Genessa facial support.

Evidence: [baseline files](../../work/three-outfit-rig-physics/baseline-1), including `baseline.json`, independent `decode-*/decoded.json`, command/exit logs, `legacy-track-conflicts.json`, `native-coverage.json` and `installed-locomotion-manifest.json`. Reproducible inventory tool: [three_outfit_baseline.py](../../tools/diagnostics/three_outfit_baseline.py). The audit did not modify installed files, settings or saves.

## Work remaining

| Workstream | Current state |
| --- | --- |
| Nine-variant provenance | Installed hashes and 16 editable mesh identities pinned; all 790 migration paths have source candidates or alias recipes. Final Blender-to-cook recipe comparisons remain |
| Mesh/weights/contacts | All 18 current normal/facial meshes pass normalized-weight and influence-index checks. Genessa world rear-panel repair accepted on the tested appearance; menu clipping explicitly accepted for now. Continuous contact and both-variant coverage remain |
| Unholy facial support | Two adapters, 21 presets and six facial performances integrated. User accepts Iris Out faces/eyes and attack/aim interruption while switching Eve, Genessa and Commander White. Exact variants unspecified; both Genessa variants and CINE still need explicit coverage |
| Shared organization | Four coordinated packages installed. CW integration audit resolves 864 packages; 426 native catalog and 1,396 outfit checks pass. Public IDs and legacy aliases retained; earlier 28 dependency scenarios pass. Clean/upgrade documentation and CINE live acceptance remain |
| Locomotion | All 63 native alternatives installed, including carriers for 60 Idle Animation choices. Black Hole and Iris Out play and interrupt on the tested original human appearance. Remaining visible contact, continuous motion and CINE review pending |
| Gameplay regressions | User accepts cross-character Iris Out interruption, first form-switch summon and Customize recovery. Full outfit/lifecycle matrix remains |
| Hair | Genessa unused-solver candidate is installed; matched cost remains. CW separate-strand trial is installed and the user reports it looks fine. Both normal/facial regular and scene graphs pass paired component lifecycle checks. Two source-visible left-bang head-turn intersections improve but remain. CINE/doubles, full lifecycle and measured gameplay cost remain; see [CW study](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/hair-head-clearance-study.md) |
| Performance | Current package sizes recorded; matched gameplay captures pending |
| CW walking feet | User confirms tilt with boots shown and hidden. Twelve moving gameplay poses captured in `walking-feet-live4`; four reconstructed bare-foot poses reviewed from front/side. The gameplay leg rotations differ from the standalone gait captures, so native blend/foot-correction values are the next comparison. The private AnimationRelative trial remains unadopted; no foot repair is installed. See [foot investigation](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/walking-foot-report.md) |
| Trebuchaxe report | Low priority, queued and unverified |

Next: investigate the reported CW walking-foot tilt, then complete CW
CINE/doubles and matched performance checks, retaining the Genessa
unused-hair-solver acceptance gap. Native conversion authoring
is complete; remaining native work is validation, not conversion. Preserve the
accepted world cloth correction and current core. Package destinations, the
nineteen stale mappings and per-entry native coverage are recorded; do not
repeat those investigations without changed inputs.

## Runtime texture guard after migration

The runtime review found one remaining path-sensitive texture check in the
doubles material setup. It rejected uncopied placeholder textures under the old
shared root but did not recognize the migrated Astral root. Both layouts now
reject those placeholders after material parameter copying. Character textures,
other shared textures and similarly named directories remain accepted by this
check. Material graph compatibility checks are unchanged.

`astral-runtime-namespace2` records the passing Astral adapter regression test
and Windows C++23 core build, with a hash-verified candidate DLL. This supersedes
the core snapshot in `dependency-runtime2`; catalog and asset payloads are
unchanged. Facial and body playback resolve their adapter paths from the checked
catalog rather than hardcoded character directories. Live migration and
rendering remain unverified. Nothing from this goal has been installed.

## Dependency and partial-upgrade handling

`catalog-namespace3` adds the declared shared dependency to each migrated outfit.
The shared pack provides `eins0fx.shared.author_namespace.v1`, using its existing
`css.locomotion` package identity. Asset payloads, paths, adapter revisions,
customization recipes and facial curves are unchanged from `catalog-namespace2`.
See [the package format](../css-packages.md#shared-asset-dependencies) and
[upgrade requirements](../asset-organization.md#coordinated-upgrade-requirements).

The runtime resolves these declarations after catalog validation and public-ID
conflict rejection. Missing/wrong/duplicate/rejected providers reject their
dependent catalogs with readable install/update/repair messages. Rejection
propagates through dependent packages without hiding unrelated outfits. The
scanner now rejects multiple manifests inside one PAK, matching the documented
one-package identity. It performs no per-frame dependency work and does not
rewrite saved state. The existing reconcile path retains a missing saved look
so reinstalling its package can restore it.

`dependency-upgrade2` passes 28 actual native-loader scenarios: malformed
declarations, conflicting providers, chained requirements, complete cycles,
compatible provider updates, missing/old/duplicate shared packs, the unchanged
legacy installation, the complete migrated installation, and each partial
outfit upgrade. The unchanged old set retains seven existing facial bindings;
the migrated set resolves all nine. Mixed old outfits/new library do not acquire
unverified facial replacements. Such a partial set is not claimed to have full
playback/facial compatibility; the coordinated upgrade replaces the matching
outfits as well as the shared packs.

`dependency-runtime2` records a successful Windows C++23 core build, all 23 host
suites and the 1,380 packaged catalog checks against `catalog-namespace3`.
Its hash-verified `css_core.dll` is staged in that directory and is not installed.
`dependency-parser1`, `dependency-upgrade1` and `dependency-runtime1` are earlier
passing checkpoints; the second upgrade/runtime reports are authoritative.

Cleanup replaced 136 byte-identical older catalog resources with links to the
current candidate, preserving their original paths and hashes. The receipt
`cleanup-catalog-resources1.json` records 82,617,888 logical bytes and observed
free-space figures. Manifests, packed evidence and installed packages remain.

## Combined catalogs and final container identities

`catalog-namespace2` assembles the three outfit catalogs with the consolidated
shared library. Its four metadata PAKs preserve the original outfit resources,
controls, variant IDs and template data while updating active object paths.
The shared catalog keeps all 63 animation IDs, adds the migrated custom-rig
walk/jog/sprint alternatives, and retains legacy aliases alongside the migrated
aliases. All three facial rigs keep their expression/curve data. Five playback
adapters and nine facial adapters bind to the newly verified outfit revisions.
The 735 package owners are unique, and every active CSS object reference resolves
to an export in the candidate stages. PAK extraction matches staged metadata and
resources byte for byte.

An independent combined IoStore audit mounts only the four candidates and the
base-game containers. All 735 packages, their public imported objects and 106
catalog references resolve, with zero failures. Legacy outfit/locomotion/Astral
containers are absent from this mount. See `combined-imports.json` and its
recorded command/exit code. This proves the combined cooked dependency check,
not live execution.

The native `--audited-namespace-pack` audit passes 1,380 checks: all nine
variants, 99 bundled choices, 540 CINE body-clip resolutions, nine facial
bindings, saved-choice serialization, deduplicated menus, and rejection of
incorrect adapter revisions. All 23 freshly linked host suites also pass.
Commands, input hashes and limits are in `catalog-namespace2/validation.json`.
This is offline catalog coverage, not proof of rendering, gameplay or performance.

Before that assembly, Eve and Commander White's independent trials were found
to share the same internal container ID, derived from their common temporary
filename. `finalize_outfit_namespace_container.py` rebuilds them under their
final distinct names. `eve-final-container1` preserves all 509 asset chunks and
309 decoded packages; `commander-final-container1` preserves 242 chunks and
172 decoded packages. Chunk content hashes, sizes and package-store dependencies
match, and independent full shader/export readback matches each accepted trial.
Their IDs are respectively `0daa14f502c7e63d` and `d4fda149b963d171`. Renaming the
old files alone would not have repaired that collision.

The superseded temporary `.ucas` files were removed only after those comparisons
passed. Their stages, headers, source receipts and decoded evidence remain.
Use the final-container directories for subsequent bindings and mounts.
Another 391 byte-identical Commander White material-stage files now link to the
complete accepted stage; previous paths and hashes still resolve, and the
observed free-space increase was 986,550,272 bytes. Cleanup receipts are
`cleanup-commander-duplicate-stage1.json`, `cleanup-eve-trial-container1.json`
and `cleanup-commander-trial-container1.json`.

`catalog-namespace1` failed because the PAK readback parent directory was absent.
The corrected second trial passes. The first trial's generated metadata/PAK was
removed; its logs and failure note remain.

Outstanding migration checks: legacy outfits resolve their saved animation
identities, but older facial/playback adapter dependencies still need a tested
upgrade route. The old locomotion catalog cannot coexist unchanged with this
catalog because its public IDs collide. A missing shared pack also removes
actual shared assets, so the metadata fallback test alone does not establish
playability without that dependency. Finish clean/upgrade handling, combined
runtime loading and live acceptance before installation or release. Nothing
from this goal is installed.

## Astral runtime namespace support

`astral-runtime-namespace1` records the C++23 runtime update for old and new
Astral paths. It loads the author-owned pose when present, otherwise the legacy
pose, and resolves all companion materials within that chosen layout. Missing
both poses reports the shared-pack installation requirement. Missing companions
do not trigger a mixed-pack fallback.

Eight explicit material-root aliases match the namespace map. Source graph GUID,
blend mode, culling, static-switch and required texture checks remain in place.
Both hair aliases keep the existing albedo requirement. The pose is retained
before collecting material pointers, so its blocking load cannot invalidate an
already-collected raw pointer list. The development probe uses the same selection.

The final Windows build exits zero and all 23 host suites pass, including
namespace boundaries, unknown authors, all 13 material definitions and masked
Uber alternatives. The 27 runtime shared object paths match the migration map.
`validation.json` records the candidate hash. This DLL is not installed; live
validation with old/new packs and Commander White's material readback remain.

## Eve material and texture migration

`eve-material-namespace1` preserves the 68 cooked material instances.
`eve-texture-namespace2` relocates 204 Texture2D headers without recooking
pixels. All 2,322 inline/streamed mip payloads and decoded texture settings
match their pinned original container after path normalization; the candidate
import audit passes. Original source/container hashes remain unchanged.
The writer's 68-instance regression check and truncated-bulk rejection pass.
See [the material migration notes](outfit-material-namespace.md) for receipts
and the supported resource layouts.

`eve-hair-editor1` passes migration and fresh reload for the hair parent and its
three texture dependencies. `eve-hair-cook1` uses the pinned v1.4.0 renderer
settings. `eve-material-pack1` combines that parent with the preserved instances
and textures: 273/273 decoded packages match, including shader maps; 405 texture
export/bulk files are byte-identical, and 539 imports resolve. This is a private
material subset, not a complete installable outfit. Its cook Wine prefix and
byte-identical readback export/bulk copies were removed with cleanup receipts.

The registry-only Eve dependency audit finds 309 selected packages (308 Eve
plus the shared base skeleton) and a 316-package dependency closure. The hair
parent has three texture dependencies. `eve-rig-dependencies2` contains that
read-only audit. The first registry attempt failed before starting Unreal
because a new bind destination could not be created under a read-only mount;
the second uses separate read-only child mounts and succeeds.

Completed editor trials' disposable DDC/cache/tmp directories were removed
after checking terminal receipts and active processes. The cleanup receipt
`cleanup-completed-editor-caches1.json` records 1,631,853,482 logical bytes.
Accepted source/cooked candidates, original inputs and build modules remain.
No pack or DLL from this goal has been installed.

### Eve rig batch in progress

`eve-rig-editor1` stages all 309 selected packages and 316 hash-pinned source
dependencies in independent copies. The first audit stopped before renaming:
it treated every declared material slot as a required render section. All
316 original and staged files were rehashed unchanged after that failure.

The cooked baseline confirms six meshes have unused material slots. The audit
module now supports complete group/section enumeration with an empty slot
selection, while keeping strict checks for named selections. Its targeted
Planet Diving check passes: 29 declared materials, 27 render sections and 29
source groups covering all 175,142 triangles. This fixes the verifier; it does
not change outfit geometry or material slots.

`mesh-namespace-module2/build-exit.json` records the successful C++20 build.
The second migration attempt passed the section-selection check, but ran out
of disk space during rename/save. It was terminated after an ordinary termination
request failed to stop it; `eve-rig-editor1/exit.json` records 137. This candidate
is rejected. All 316 original authoring inputs were rehashed unchanged in
`attempt2-source-check.json`; no candidate was installed.

`cleanup-eve-enospc1.json` records removal of the rejected private Content,
reproducible original-container extraction, and three completed cooks' DDCs.
It preserves logs, hashes, decoded comparisons, source containers and accepted
material candidates. The 5,735,123,231 logical bytes removed restored about
3 GB of measured free space, from 2.5 GB to 5.6 GB. Reflinked copies do not
necessarily reclaim their logical size.

`eve-rig-editor2` restages the 309 selected packages and 316 dependencies from
the original hash-pinned sources. It uses module2 and an independent reflink
copy of the warm DDC. `space-check.json` records the preflight space estimate;
`command.json`, `console.log` and the eventual `exit.json` record the retry.
The retry and fresh-process reload both pass for all 309 selected packages.
The reload ran with staged legacy paths removed from Content; all 316 original
inputs remain hash-identical. `eve-rig-cook1` then saved all 36 rig packages for
Windows with exit 0. Complete outfit assembly and independent cooked mesh
comparison remain required before acceptance. Nothing has been installed.

### Large Commander White mesh audit

`commander-rig-editor1` contains 172 selected packages and 184 independent,
hash-pinned inputs. Its first attempt failed before renaming: the render audit
tried to serialize more than 2 GB through UE's FString memory writer. The
184 originals and private staged files were rehashed unchanged afterward.
This was an offline audit failure, not a game or outfit failure.

`mesh-namespace-module3` adds opt-in `array-blake3-v1` snapshots. Large render
and source arrays are hashed incrementally, one complete row at a time. The
ordinary detailed snapshot remains available. Geometry, normals, tangents,
UVs, colors, indices, weights, cloth mappings and morph deltas remain covered;
there is no sampling or reduced mesh. Material-slot validation is unchanged.

The C++20 UE 5.6.1 build and complete C4 source/render audit pass. An independent
Python BLAKE3 implementation compared the exact UTF-8 arrays from the detailed
snapshot: 39 render arrays containing 1,311,333 rows and 36 source arrays
containing 209,281 rows match. The selected sections include cloth mappings;
missing and duplicate slot requests still fail. Evidence is in
`commander-rig-editor1/array-digest-probe/independent-comparison.json`.
The migration retry explicitly selects the new snapshot format. Its save and
fresh reload now pass for all 172 selected packages, with legacy paths absent
and all 184 original inputs unchanged. `commander-rig-cook1` passes its
12-package Windows rig cook. `commander-mesh-comparison1/comparison.json`
matches both C4/E9 meshes against the installed baseline, including reference
poses, geometry, indices, colors, weights, cloth mappings and every morph.
Material migration subsequently passed, as recorded below. Live acceptance remains.

### Complete Commander White private assembly

`commander-material-pack2` preserves all 160 material/texture packages, including
929 texture mips and both compiled parents. All decoded fields/shader maps match
and 428 imports resolve without the original outfit mounted. Its first trial
revealed six numbered package FNames missed by raw-string relocation. The writer
now retains FName indices/numbers while mapping their stored base names and
rejects conflicting or unmapped siblings. Three Rust regressions and eight
byte-identical files across the parent, texture and blueprint modes pass.
See [material migration notes](outfit-material-namespace.md).

`commander-rig-pack1` passed dependency checks but rejected two source-blueprint
differences: `CW_BodyGeometryJson` had become `CSSBodyGeometryJson` in B2/B3.
`commander-secondary-namespace1` instead preserves both released compiled
blueprints, with all 13 decoded exports matching after namespace normalization.
No property rename is accepted as a side effect of moving folders.

`commander-rig-pack3` passes the complete 172-package comparison (171 outfit
packages plus the shared base skeleton) and all 598 imports. Its two mesh
payloads match the cook already independently compared against the original
C4/E9 render, skin, cloth and morph buffers; `mesh-evidence.json` pins that chain.
This is a private assembly, not an installed release or live physics test.

Pack2 ran out of disk space during duplicate readback extraction. Its incomplete
files and pack1's superseded stage were removed only after matching retained
material/cook/blueprint inputs, recorded in `cleanup-commander-disk-full1.json`.
The successful retry checked room for three payload copies before starting.
Pack3's verified duplicate readback payloads were then removed, keeping its
stage, container, headers and decoded evidence. Their 1,382,675,238 logical bytes
are recorded in `commander-rig-pack3/readback-payload-cleanup.json`.

Completed Eve reload/cook caches and the obsolete failed Eve editor cache
were removed with receipts, preserving candidates, original inputs and logs.
Module3's rebuildable Intermediate directory was also removed after its build
and audit passed; source and binary hashes were checked unchanged. These
cleanups remove reproducible work data, not outfit or release content.

### Eve compiled secondary graph

`eve-rig-pack1` resolves its imports and matches 308 of 309 decoded packages.
All 22 mesh buffer comparisons also pass, including weights, cloth mappings
and morphs. The remaining mismatch is `ABP_Secondary`: recompilation changes
generated property identities, graph layout and the mutable-data export. This
candidate is rejected rather than treating those differences as harmless.

`eve-secondary-namespace1` preserves the released compiled blueprint. The
header writer accepts this explicit animation-blueprint mode, preserves name
indices and export payload bytes, rejects unsupported export classes and
requires both unchanged and inverse header roundtrips. Independent container
readback matches all seven original exports after path normalization. The
three referenced Control Rigs are supplied as explicit dependencies. The first
dependency-incomplete readback is retained separately and is not acceptance.

Writer regression checks pass for 68 material instances and three textures,
with export/bulk bytes unchanged. A material submitted as an animation blueprint
is rejected. `eve-rig-pack2` combines the preserved blueprint with the verified
materials and cooked rigs. Its final comparison passes all 309 packages
(308 Eve plus the shared skeleton), with all imports resolved. All 44 mesh
payload files from the independent 22-mesh comparison are unchanged in this
corrected assembly; `mesh-evidence.json` records that linkage. This is private
package acceptance, not a gameplay or clean-upgrade install check.

Assembly payloads use `/mnt/eins0fxE/CSSAuthoringWork/three-outfit-rig-physics`
because the system drive lacks space. Project work links preserve the usual
receipt paths. Rejected pack1 stage/readback/container payloads were removed
after retaining comparisons, hashes and source inputs. The first assembly's
reports and mesh evidence remain; nothing has been installed.

After the corrected Eve assembly passed, 509 duplicate readback payload files
(1,953,661,365 logical bytes) were removed. Canonical stage/container payloads,
readback headers, decoded comparisons and receipts remain. Completed Commander
editor/cook DDCs were also removed. The cleanup receipts are stored beside
their trials; logical sizes are not reported as physical space reclaimed.

`eve-mesh-comparison1/original-decoded.json` records independently decoded
geometry, skin weights, cloth mappings and morph buffers for all 22 skeletal
mesh packages currently supplied by Eve and its shared facial replacements.
The migrated counterpart has not yet been compared.

`commander-rig-dependencies1` completed its read-only registry query with exit
zero: 172 selected roots, including the shared base skeleton, and 184 packages
in the dependency closure. File validation found the historical `PA_ClothC8`
dependency of `SK_FullB9` absent from cw270. Its original cw7 authoring file is
retained and hash-pinned in `commander-historical-dependency1/source.json`.
The isolated registry check identifies it as `/Script/Engine.PhysicsAsset`
with no package dependencies. It is an editor dependency, not a replacement
for the released outfit's C13 physics asset.

The registry probe now checks on-disk asset identities as well as dependency
edges. Previously, a missing package could appear as an empty dependency list.
`commander-dependency-negative1/verification.json` verifies that the real physics
asset is recognized and a deliberately absent package is rejected with an
explicit missing-package report. The
[UE 5.6 registry API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AssetRegistry?application_version=5.6)
documents the on-disk query used here.

`commander-rig-editor1` now stages 172 selected packages and all 184 dependencies
in independent, hash-checked copies. Its migration has not started.
`commander-mesh-comparison1/original-decoded.json` independently records the
released C4 and E9 mesh buffers, including weights, cloth and morphs; its
readback exits zero. Candidate comparisons remain pending.

Cleanup removed the superseded standalone Eve texture stage/container and
rejected Genessa material cook1/pack1 payloads only after verifying the retained
full Eve material stage and preserved rejection evidence. The per-file receipt
is `cleanup-superseded-material-trials1.json` (2,344,561,871 logical bytes).

## Genessa rig and mesh namespace migration

`genessa-rig-editor2` passes isolated UE 5.6.1 rename/save and fresh-process reload
for 101 Genessa packages plus the shared base skeleton. The dependency staging
contains 152 hash-pinned original files, all unchanged after migration. No legacy
redirector files remain in its Content tree.

The thirteen non-material Genessa packages comprise four animation blueprints,
three Control Rigs, three physics assets, the private wing skeleton and two W3
meshes. The checks include mesh bind transforms, source/render geometry and
weights, morphs, reflected cloth/configuration data, non-reflected tether arrays
and cloth transition mappings. References follow the explicit migration map.

Three Control Rigs needed narrowly scoped comparison exceptions for generated
editor data: the graph's regenerated structure hash, empty modular-controller
helpers named from the old outer path, and transient property-bag type names.
Stored values and graph fields still compare. The retained mismatch reports
show the differences; `rig-cache-comparison.json` also verifies that changed
memory values and nonempty controller properties fail the comparison.
See the [audit module notes](../../tools/authoring-probes/rig-audit/README.md)
for the pinned engine-source basis of those exceptions.

Evidence is under `work/three-outfit-rig-physics/`: `genessa-rig-dependencies1`,
`genessa-rig-editor2/{migration,reload,source-validation}.json`, the successful
`reload3-command.json` / `reload3-exit.json`, and the C++20 build receipt in
`mesh-namespace-module1/build-current3-exit.json`. Earlier failed reload logs
remain alongside the successful third check. This is editor-source evidence;
cooked equivalence and gameplay acceptance remain pending.

The rejected first trial's asset/cache copies were removed after preserving
its input hashes, source snapshots, mismatch and logs. The cleanup receipt
records 764,127,775 logical bytes, not a claim about physical free space.
No installed pack, DLL, settings or saves changed.

### Cooked Genessa assembly

`genessa-rig-cook1` cooks the thirteen rig/mesh packages plus the common skeleton
under the original outfit's renderer settings. `genessa-rig-pack2` combines
those thirteen packages with the 88 verified material payloads. Its private
101-package container mounts alongside `shared-namespace1`, with no original
outfit or locomotion package in the candidate dependency mount.

All 102 decoded packages (including the shared skeleton) match their original
fields after explicit path normalization. The dependency audit resolves 260
imports and 102 requested exports. The first assembly's catalog check incorrectly
requested Blueprint asset names instead of their cooked `_C` exports; its reports
remain in `genessa-rig-pack1`, and the corrected assembly passes. No asset change
was needed for that verifier error.

The independent `AssetReadback --mesh-digests` check also matches both W3 meshes'
full bind skeleton, 160,794 render vertices each, positions/normals/UVs/skin
weights, indices, colors, sections, cloth mappings and all 22 existing morphs'
decoded deltas. This complements the reflected package comparison rather than
inferring geometry equality from package metadata.

Evidence: `genessa-rig-pack2/{verification,comparison,import-check,mesh-comparison}.json`.
The .NET 10 reader build succeeds; its vulnerability-cache lookup reports NU1900
because that cache is read-only. The cook exits zero. Duplicate first-assembly
payloads were removed only after matching the accepted assembly; the cleanup
receipt records 392,472,198 logical bytes. This candidate still uses the original
W3 facial geometry, and is not a release or installed package. Final facial
integration, runtime/catalog consumers and live validation remain.

## Genessa material namespace migration

The [material migration notes](outfit-material-namespace.md) document the native
skin/eye proxies and renderer settings that must not be lost during organization.
Thirty-three material instances use preserved cooked export bytes and reversible
header relocation. Seven authored parent materials and 48 textures pass editor
rename/save/reload and Windows cooking under the outfit's original renderer
settings. The 88-package candidate matches all released decoded fields, including
shader maps, after explicit path normalization. All 96 texture export/bulk files
are byte-identical. Its import audit passes with only base-game and candidate
containers mounted.

Evidence: `genessa-material-namespace2`, `genessa-material-editor1`,
`genessa-material-cook2` and `genessa-material-pack2` under the rig workspace.
The first cook was rejected because its default project settings omitted the
released unlimited-influence skin/cloth shader variants. The corrected cook
pins the source RendererSettings and preserves them. A changed baseline-container
hash also rejects before any staging output is created.

This completes the offline material subset, not the full Genessa migration.
Its 13 rig/mesh/adapter packages, facial candidate integration, final catalog
and runtime consumers still need coherent assembly and verification. The old
adapters already have isolated migration evidence, but their final references
must follow the migrated outfit dependencies. No installed pack, DLL, setting
or save changed.

## Namespace parser change

[Organization convention](../asset-organization.md) documents the author/project roots. The first supporting CSS change accepts shared animations/carriers/adapters under `/Game/CSS/<author>/Shared/Locomotion/` and character facial replacements under `/Game/CSS/<author>/<project>/Faces/`, retaining the legacy roots. Tests cover a second author, malformed namespace boundaries, native idle resolution and unchanged revision checks. The C++23 host build and all 23 host suites pass. This is catalog validation coverage, not migrated asset or live-game acceptance. No new DLL has been installed.

### Base-skeleton namespace compatibility

The runtime accepts the verified `/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base`
object alongside its legacy `/Game/CSS/Shared/SKEL_Base` identity. Evidence is
the exact normalized Skeleton comparison in `migration-cook6/comparison.json`
and the subsequent shared-pack payload check. The change preserves the existing
directed base-to-Genessa-wing animation mapping and rejects its reverse.
It does not infer compatibility from an author's folder name.

The C++23 host build and all 23 registered host suites pass. Added cases cover
old and new base targets, legacy saved IDs and canonical IDs, shared preference,
bundled fallback, restoration to native shells and rejection of lookalike paths.
These are resolver checks, not live validation of the full migration. No DLL or
asset pack was installed. Astral consumer paths and remaining outfit migration
are still pending.

### Wing-skeleton namespace compatibility

The cooked Genessa comparison now establishes the private wing alias under
`/Game/CSS/eins0fx/UnholyGenessa/Skeletons/SKEL_Wings2`. CSS recognizes both old
and new wing identities for appearance changes and playback. Base-to-wing
playback remains directed: wing-only clips cannot target the base rig.

Both CSS idle selection and CINE performance selection prefer an exact
skeleton, then its verified namespace alias, then a compatible base alternative.
This ordering preserves wing-specific clips across old/new catalog combinations.
Tests cover those priorities, old saved IDs, shared preference, bundled fallback,
native appearance restoration, reverse-remap rejection and unknown-author
lookalikes. The C++23 host build and all 23 host suites pass. Build/test evidence
is in `genessa-rig-pack2/host-*.json` and logs.
No new DLL is installed; game acceptance remains separate.

## Storage and cleanup

Initial free space was approximately 12 GB on the system drive and 13 GB on the game drive. Decode mounts use symlinks. The namespace trial now has isolated editable copies and two small 32-package cooks. Eve reference dependencies were copied with Btrfs reflinks, not mutable hardlinks. Prior video research removed only its temporary model and Python environment; source videos and evidence remain intact.

## First cooked namespace trial

The updated [migration map](../../work/three-outfit-rig-physics/namespace-2/asset-and-catalog-map.json) keeps the private wing skeleton in `UnholyGenessa/Skeletons`. Ownership is now 305 Eve, 130 Genessa, 170 Commander White and 185 Shared packages. It does not move Genessa-specific wing bones into the shared foundation.

An isolated UE 5.6.1 editor project migrated the common skeleton, 27 Eve animation sequences and four blend spaces. The original source tree was mounted read-only. Animation-data snapshots cover 566,868 source bone transforms, frame counts/rates, model GUIDs, curve/attribute counts and root-motion settings. Skeleton snapshots cover 386 raw bones, nine virtual-bone definitions and 82 sockets. A fresh process reloaded the saved candidates with old redirector files removed and verified the new skeleton and blend-sample references.

The first minimal cook was rejected: saving animations without their reference mesh cleared `RetargetSourceAssetReferencePose`. The second trial includes an independent editable copy of the reference dependency and explicitly refreshes this pose before renaming. Twenty clips had 388 transforms; the current skeleton requires 395. The refresh preserves the first 379 body transforms and last nine virtual transforms, inserting seven identity helper transforms. Seven newer idles already had 395 transforms and remain unchanged. Source animation keys did not change. The effect of the old reference-pose mismatch in gameplay has not been established.

The corrected 32-package cook passed an IoStore conversion/readback with identical export payload bytes. All 27 decoded sequences retain 395 reference transforms and virtual track indices 386-394. The nineteen movement sequences independently compared against the installed Eve decode differ only in package/skeleton references and the reference-pose refresh. The other eight clips have source-snapshot and cooked structural checks, not independent installed decoded comparisons.

Evidence: [editor trial](../../work/three-outfit-rig-physics/migration-editor3) (`migration.json`, `reload.json`, `retarget-validation.json`); [corrected cook](../../work/three-outfit-rig-physics/migration-cook2) (`comparison.json`, `pack/roundtrip.json`); [rejected cook](../../work/three-outfit-rig-physics/migration-cook1/comparison.json). Reusable probe: [migrate_namespace_trial.py](../../tools/authoring-probes/animations/migrate_namespace_trial.py) with the [editor audit module](../../tools/authoring-probes/rig-audit/README.md).

This is an offline migration check, not a release or game acceptance. No installed DLL, outfit, pack, setting or save was changed. Exact runtime skeleton compatibility, Astral adapter paths/material identities, catalog dependencies and legacy outfit behavior still need coherent migration and verification.

### Expanded shared-animation trial

The expanded editor trial covers 116 packages: 107 animation sequences, eight
blend spaces and the common skeleton. Source snapshots cover 8,568,856 keyed
transforms. They survive rename/save/reload, and all 409 original dependency
files retain their hashes. A private dependency subset avoids loading unrelated
historical meshes during Unreal's referencer updates.

The third Windows cook is **rejected**, despite passing its container roundtrip.
Independent compressed-stream readback caught the main authoring project's
native-human placeholder: 20 native sequences cooked with 182 mapped tracks
instead of 1,208, and four native blend spaces acquired its wrong skeleton GUID.
This establishes why source snapshots and a successful cook alone are inadequate.
The installed packages were never replaced.

The next staging run uses the exact native skeleton preserved by the earlier
`native-binding-v6` receipt, SHA256
`9497cd869c7e39c368091acc2c6ce6202ed1bff54383d2490415815712b69de0`.
It replaces the placeholder only inside the private staging tree. Both the
source receipt and copied reference are checked before use and on reload.

Of 107 sequences in the rejected cook, 68 retain identical compressed bone and
curve streams. Nineteen Eve movement sequences also have changed compressed
bone streams alongside the documented retarget-reference refresh. Their raw
keys remain unchanged, but full decoded motion comparison is still required.
The earlier 32-package structural comparison did not check these stream bytes.

Evidence: `migration-editor5/source-validation.json`,
`migration-cook3/initial-differences.json`, both independent decoded reports in
that cook directory, and the corrected reference in `migration-editor6/input.json`.

The corrected `migration-editor6` / `migration-cook4` batch passes fresh reload,
Windows cook and payload roundtrip. Ninety-six of 116 packages have identical
decoded fields after namespace normalization, including all 20 native sequences
and four native blend spaces. Eighty-eight of 107 animation streams are byte
identical. The corrected native dependency exposes 1,199 raw bones and nine
virtual bones; source snapshots now cover 9,342,460 keyed transforms.

The remaining 20 packages are the Eve clips with the reference-pose refresh.
All 754 source-key frames, comprising 292,552 mapped transforms, were decoded
independently through native ACL and compared with raw Unreal local poses.
Every clip passes the existing review limits (0.05 cm translation, 0.25-degree
quaternion-distance equivalent, 0.001 scale). Measured maxima are 0.009725 cm,
0.206 degrees and 0.00002659 scale. This covers all mapped virtual tracks as
well as body tracks, but does not establish between-key blending or gameplay
acceptance. See `migration-editor6/eve-motion-comparison.json` and
`migration-cook4/comparison.json`.

Rejected editor5/cook3 asset copies and caches were removed after preserving
logs, decoded comparisons and a per-file cleanup receipt. The receipt records
1,320,489,845 logical bytes; reflinks mean this is not a physical-space claim.
See `cleanup-rejected-native-placeholder.json`. Current candidates, original
assets and installed files remain intact.

## Editable source audit

### Outfit-owned playback adapters

The initial ownership map grouped five playback adapters with shared locomotion.
Inspection of their creation recipe shows they retain outfit-specific body,
hair, wing and foot rig nodes. The revised `namespace-3` map assigns the three
Eve adapters to `EveSB/Animations/Adapters/`, the Genessa adapter to
`UnholyGenessa/Animations/Adapters/`, and the Commander White adapter to
`CommanderWhite/Animations/Adapters/`. Counts are now Eve 308, Genessa 131,
Commander White 171 and Shared 180. All 790 source-provider choices and 63
public animation IDs remain unchanged; unknown character adapters now require
an explicit owner rather than silently entering Shared.

CSS accepts those author/project adapter paths while retaining old paths and
exact source revision, source class and skeleton checks. The C++23 host build
and all 23 test suites pass, including resolution for two author namespaces and
rejection of malformed paths, missing class suffixes and mismatched revisions.
This runtime change is not installed.

`migration-editor7` extends the private batch to 124 assets. All 116 earlier
snapshots are unchanged. Five blueprints and three compression settings pass
reflected-property and graph-pin comparisons, then fresh reload without old
redirectors. All 421 input files retain their hashes. The six rig dependencies
missing from the main authoring tree use their pinned outfit-owned originals.

The first expanded cook (`migration-cook5`) rejected the new codec paths because
the authoring commandlet accepted only the legacy locomotion root. The isolated
`namespace-cooker1` build adds author-first shared paths while retaining exact
ACL codec type and ownership checks. The corrected `migration-cook6` passes
Windows cook, payload roundtrip and all 547 imported references across 124
packages with the explicitly pinned baseline dependencies mounted. Independent
readback matches 104 installed packages exactly after namespace normalization,
including all five adapters and three codec settings. The other 20 remain the
documented Eve reference-pose corrections. All 107 candidate animation streams
match the prior checked candidate byte-for-byte, preserving the applicability
of its full-key motion evidence. See `comparison.json`, `import-check.json` and
`motion-evidence-reuse.json` in that cook directory. This is still a mixed-owner
test container; final split packaging, full dependency migration and live
acceptance remain. Original authoring binaries and installed files were not replaced.
The rejected cook's 3,250,659 bytes of cache/partial output were removed with
logs retained; see `cleanup-rejected-codec-root.json`.

The [current mesh/source report](../research/2026-10-08-editable-mesh-audit.md) records all nine outfit meshes plus seven active facial replacements. The read-only editor audit checked 11,608,800 render vertices and 1,220 morph entries without basic weight/index/nonfinite failures. It preserves the distinction between mesh bones and Skeleton helpers. Eye, cloth, material and post-process identities are recorded per mesh; visual behavior still needs validation.

All 790 migration destinations now have editable candidates or alias recipes. Sixty-one are historical cooked animation aliases. The two differing source candidates are Eve's hair instances, resolved against their v1.4.0 authoring receipt. This avoids reverting its accepted hair while reorganizing assets. Final source-to-installed cook equivalence is still pending outside the first movement trial.

Cleanup removed 2.14 GB of reproducible extra namespace-trial mesh copies and cache (logical size), preserving original sources and the accepted candidate. See `work/three-outfit-rig-physics/cleanup-migration1.json`. The new mesh audit used read-only content mounts and did not install anything.

## Genessa facial candidate

Character-specific implementation lives in the [Unholy Genessa authoring notes](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/authoring/facial-controls-notes.md), with tools in that mod's `tools/` directory. The sealed mask mouth and poor eyelid transfer were reproduced visually. The current candidate adds seven lip-seam vertices, an oral surface and controls fitted to Genessa's eyelids while retaining the original neutral mask points. The head backing also needs matching expression motion; leaving it neutral reproduced mouth obstruction and cheek breakthrough.

The retained `face-articulation5`, `face-backing2` and `face-patch1` outputs cover 115 control names. The interchange preserves original face UVs, corner data and head weights. Sparse indices, finite geometry and neutral triangles pass structural checks. All six current facial performances have matching control names. Iris Out's 23 existing curves were rendered for 501 frames at 30 fps, with inspected neutral and expression samples. This is offline clay-head evidence, not an installed or fully validated face.

The editor integration below follows this head-only candidate. Original Genessa source hashes remain unchanged. Nothing was installed. Initial cleanup removed 17.9 MB of rejected intermediate files (logical size), with reports and useful visual evidence retained.

### W3 editor integration progress

Both W3 source descriptions match the facial patch: 1,303 mask points and all
face UVs/normals match exactly; 8,976 backing points map within 0.000007643 cm.
Original polygon checks account for 68 alternate quad diagonals and four tiny
exporter-omitted triangles. The backing retains its native triangulation.

The outfit-owned C++20 module now builds private facial replacements under
`/Game/CSS/eins0fx/UnholyGenessa/Faces/`. Both candidates add 115 expression
controls and retain all 22 original morphs and their delta counts. Skeleton,
physics-asset and post-process identities agree; weight and morph index checks
pass. This does not yet prove complete cloth or body equivalence.

Intermediate evidence is Genessa `work/face-integration4`, whose editor exited 0.
It has 172,151 render vertices per variant, compared with 160,794 originally.
The oral section still has more render splits than its source points/corners
justify. The source uses MikkTSpace tangent recomputation, so authored tangents
alone do not resolve this. Next: inspect the final render basis, verify full
source/render/cloth equivalence, review materials and expressions, then cook
and wire CSS/CINE. Original assets and installed game files remain untouched.

The subsequent `face-integration6` resolves the excessive oral splitting by
ordering mesh instances to match UE 5.6.1's tangent writer/reader indexing.
Both variants now contain 162,939 render vertices, including 2,138 oral vertices
and seven independently controlled lip duplicates. All 24 unchanged render
sections match the originals exactly, including their cloth mappings and body
morph deltas. All 115 mask controls pass correspondence checks subject to the
original 0.015 cm pruning threshold; retained delta error is below 0.000047 cm.

The lip duplicates use the mask's previously zero UV3 channel to prevent render
welding. Native/Astral material use of this channel is still unverified. This is
an explicit remaining acceptance gate, alongside full physical cloth-asset
comparison, expression/material review, cooking and runtime integration. No
candidate has been installed. The complete goal remains active.

The next read-only check closes the physical cloth-data comparison for both W3
candidates. `face-integration6/cloth-comparison.json` matches all six clothing
assets per variant: reflected physical data/configuration, collision references,
bone mappings, GUIDs, LOD settings, non-reflected tethers and transition arrays.
The candidate owns separate clothing objects. A detached-copy negative check
rejects an altered simulation vertex. `cloth-audit-exit3.json` records exit 0.
The first two diagnostic attempts failed on an unavailable Python property and
an incorrectly named probe morph; neither saved or installed assets.

The probe rebuild also passes on UE 5.6.1. Its finite-data guards were exercised
on the rebuilt mask/mouth with `jaw_open`. Cleanup compressed older render
snapshots with hash-verified roundtrips and removed superseded private caches
and candidate copies, reducing logical size by 1,215,207,053 bytes. The retained
receipt is Genessa `work/face-cleanup3.json`.

Native face shader evidence is in Genessa `work/face-material-audit1`. The
readback pins `MI_Genessa_top`'s SM6 shader map
`4C3FB7ABD0DFE5893189D38F886434F002E4A8FB`. Its four GPU-skin/cloth base-pass
vertex permutations declare only UV0 (`ATTRIBUTE5.xy`); there is no UV3 input.
Its two base-pass pixel variants use only `TEXCOORD0.xy` for material UVs.
The fifth (local/manual-fetch) permutation fetches only UV0: SSA 327-333 indexes
`NumTexcoords * (VertexOffset + VertexId) + min(0, NumTexcoords - 1)`, and writes
that pair to `TEXCOORD0.xy`. `uv-review.json` pins all five disassemblies.
This supports the seam tag for those permutations. It does not establish all
render passes, SM5 or Astral shader behavior. SM5 extraction
succeeded, but DXC cannot disassemble those containers. Material review and
cooked/runtime acceptance remain open. The pixel extractor's previous outputs
remain identical after adding explicit vertex extraction.

Both candidate Windows cooks now pass independent geometry/morph readback and
an IoStore export-payload roundtrip. Genessa `work/face-cook1/morph-verification.json`
records unchanged neutral positions, primary UVs, triangle/morph indices and
compression errors within the stored precision for all 137 controls per mesh.
The backing and mouth expression checks also pass. Extra ornament/hood entries
are tangent-only changes, not positional displacement; visual shading review
remains required.

The subsequent `work/face-cook3` trial includes the outfit-owned oral material
and its three textures in Genessa's `Faces/Mouth` directory. With Eve absent
from the decode mount, both mesh material bindings resolve and all four oral
assets match original decoded fields after namespace changes. Material and
texture payloads remain byte-identical, and all five direct game dependencies
decode. A stale binding in cook2 was caught and corrected before this trial;
the final verifier rejects that previous candidate. Remaining original W3
dependencies still use legacy paths, pending the wider migration. The candidate
has not been installed or accepted in the game.

### Facial catalog and material checks

Genessa `work/face-catalog1` binds both current W3 variants to their facial
replacements using the original outfit's exact container revision. It provides
115 controls, 21 presets and six existing facial performances. Performance keys,
times and values are unchanged from the accepted source resource. The focused
C++ check passes 163 assertions, including both adapters, all presets/clips,
body-only fallback and rejection of a different outfit revision.

`work/three-outfit-rig-physics/combined-facial-catalog1` adds that rig to the
current shared metadata in a private PAK. Roundtrip readback preserves the entire
previous catalog and its external facial resources. The 52,936-byte manifest
contains 63 body entries and three facial rigs. The actual C++ package loader
passes 182 checks, including the 60 Idle Animation options and matching CSS/CINE
body resolution. All 23 host suites pass. Logs are in `genessa-catalog-host1`.
This intermediate metadata trial retains legacy payload names; it is not the
consolidated `CSS_SharedAssets_P` release or a completed namespace migration.

Genessa `work/face-preset-review1` contains 22 front-view source-head renders,
including the original. Review found no obvious collapsed eyes, sealed open-mouth
presets or cheek breakthrough in these views. These are clay renders, not game
material or whole-body acceptance.

Shader readback now covers all native face vertex passes and the opaque Astral
companion's inline code. The 24 native and four Astral GPU-skin/cloth SM6 variants
use UV0 without UV3. The seven native and three Astral local/manual-fetch variants
either fetch only UV0 or omit UV reads. Disassemblies and the exact reviewed
indices are pinned in Genessa `face-astral-shader1/uv-review.json`. SM5 bytecode
was extracted but is not disassembled by this DXC tool. Native extraction remains
byte-identical for all 56 shader resources after adding inline support.

No candidate has been installed. Remaining face checks include cooked material
appearance, dynamic ornament/hood shading, CSS/CINE playback and interruption,
both Genessa forms and doubles. Wider migration, physical deformation/contact
work and the 59 missing native animation conversions remain active.

### Facial meshes integrated into the migrated Genessa outfit

`genessa-face-namespace-editor1` adds the two verified facial meshes and four
oral material/texture assets to the isolated Genessa migration. Those six
assets already have author-owned paths. The migration script now saves and
checks such assets while renaming their dependencies, without renaming them
onto themselves or treating their presence as a legacy redirector.

All 108 selected assets pass before/after comparisons and a fresh editor reload
with the old paths removed. Both facial meshes retain 137 morphs, complete
source/render arrays and cloth custom data; their skeleton, Physics Asset,
post-process graph and materials point into the migrated namespace. Every
protected original source remains unchanged. The subsequent two-mesh Windows
cook passes with read-only source Content.

`genessa-face-namespace-pack2` combines the 101 migrated outfit packages with
the two facial meshes and four preserved oral packages. All 107 outfit packages
plus the shared skeleton pass comparison, with 326 imports resolved using only
the candidate, new shared pack and base game. Both facial meshes match the
earlier `face-cook3` candidate after path normalization, including every decoded
render, weight, cloth-mapping and morph buffer. Existing outfit exports remain
unchanged. The oral comparison includes compiled shader maps, not just instance
properties.

Pack1 incorrectly compared an old oral readback made without shader-map loading
against one with shader-map loading. Its sole mismatch was the populated
`LoadedMaterialResources` array. Pack2 reads both sides with the same mode and
compares the complete data; it does not waive that field. The two containers are
byte-identical. `assemble_genessa_face_namespace.py` records the inputs, commands,
import checks and full comparison.

The rejected trial's duplicate payloads, successful trial's duplicate readback
payloads and completed editor/cook DDC were removed with hashes and retained-file
locations in `cleanup-genessa-face-namespace1.json` (1,085,711,576 logical bytes).
Inputs, editable candidates, accepted cooked assets, containers and reports
remain. Catalog revision bindings and live facial acceptance are still pending;
this package has not been installed.

## Verified animation consolidation and saved choices

The [fourth namespace map](../../work/three-outfit-rig-physics/namespace-4/asset-and-catalog-map.json)
retains all 790 source identities but consolidates 61 documented animation
copies into their shared destinations: 308 Eve, 101 Genessa, 171 Commander
White and 149 Shared packages, 729 total. No installed package was removed.

[Independent readback](../../work/three-outfit-rig-physics/animation-alias-audit1/verified.json)
verified 54 sequences and seven blend spaces against their 31 canonical source
packages. Decoded fields, compressed bone/curve hashes and normalized blend
references agree. `audit_animation_aliases.py` records the evidence;
`plan_asset_namespace.py --verified-aliases` rejects changed input/dependency
hashes before using it. New names do not imply new motion.

The old library declared only Eve's 11 legacy aliases.
[The private metadata trial](../../work/three-outfit-rig-physics/legacy-alias-catalog1)
adds the corresponding 22 Commander White and Genessa aliases using pinned
outfit manifests and the verified copy map. It preserves all 63 public entries
and the three facial rigs. The host audit passes 742 checks, including all
99 bundled choices across the nine variants: menu deduplication, legacy and
canonical selection, saved-ID serialization, weapon visibility and fallback
when the shared pack is absent. The separate facial catalog audit and all
23 host suites also pass. Receipts are in `validation.json`.

This trial changes alias metadata only. The existing shared walk/jog/sprint
entries declare native-human alternatives; custom rigs still select their
verified bundled blend spaces. Adding the migrated custom-rig shared alternatives
and rebuilding consumers remains part of migration. This does not establish
asset loading or live-game acceptance and is not installed.

## Astral migration and shared payload assembly

`migration-editor8` extends the private batch to 154 assets with the 30 Astral
resources. Both save and fresh-process reload pass with legacy redirectors
removed. All 451 protected inputs remain unchanged, and all 124 earlier asset
snapshots/references match `migration-editor7`. Material expression properties,
instance overrides and texture source mip hashes are included in the check.

The first snapshot attempt found the empty generated mutable-data struct on
`ABP_CopyPose` disappearing during recompilation. UE 5.6.1's
`FAnimBlueprintCompilerContext::BuildDataArea` explicitly discards unused mutable
data. The audit now omits only that named generated struct when it has no
reflected fields; graph properties, pins and nonempty state remain checked.
The rejected partial save was discarded and recopied from pinned inputs.

`astral-namespace-cook1` was rejected: its Windows cook externalized shader code,
which the asset-only pack did not carry. `astral-namespace-cook2` explicitly
disables shared shader libraries, matching the accepted original cook settings.
It passes Windows cook, the 30-payload IoStore roundtrip, all 26 material variants'
SM5/SM6 skin/cloth/refraction/default/culling checks and the copy-pose template
check. `material-equivalence.json` proves all 30 independently decoded exports
match the installed Astral baseline after namespace normalization, including
shader-map data. Legacy-root validation also passes; the rejected external-shader
readback is still rejected by the updated checker. This is not live rendering.

[The assembled shared trial](../../work/three-outfit-rig-physics/shared-namespace1)
contains all 149 shared packages in `CSS_SharedAssets_P`: the 119 shared
skeleton/locomotion/codec assets from the prior cook and the 30 migrated Astral
assets. The five outfit-owned playback graphs are excluded. Every payload
roundtrips unchanged, and all 517 package imports and 149 requested exports
resolve against the explicitly mounted baseline dependencies. These are asset
containers only, not an installable release: catalog/facial dependency hashes,
runtime consumers, outfit migration and clean/upgrade/game acceptance remain.
`namespace-4/runtime-consumers.json` records 41 literal legacy-reference lines
across five CSS source files; dynamic consumers also require review.

Cleanup removed the rejected Windows cook's generated output/cache and both
disposable Wine prefixes after confirming the cooks had exited. Small receipts,
logs, sources and decoded evidence remain; accepted cooked assets and the shared
trial remain. See `cleanup-astral-namespace-cooks.json` and
`astral-namespace-cook1/rejected.json` for logical byte counts and observed free
space. Reflink logical sizes are not claimed as physical space reclaimed.
Nothing from this goal has been installed.

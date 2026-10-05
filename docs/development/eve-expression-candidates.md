# Eve expression candidates

2026-10-06. Authoring candidates only. No game files were replaced.

`export_facial_shapes.py` extracts the 99 facial shapes needed by the 21 supplied
ATOOL photo expressions from `CSS_SeduXtress_HeelSupportsV45C.blend`. It uses the
existing exporter's fitting and vertex filtering, then checks the body point
order against the accepted heel-support export. Body positions match exactly.
Existing body and hand deltas differ by at most 0.000078 cm where the older
sparse export omitted tiny values. All original input hashes remain unchanged.
The two jaw curves are matched case-insensitively; exported names preserve the
source animation curve spelling. Authored negative weights are retained.

`CSSFacialLibrary` adds the shapes to detached mesh descriptions, validates all
input before modifying a candidate, then lets UE 5.6.1 build the morph data.
It accepts only commandlet-owned assets under `/Game/CSS/AnimLab/SK_Face_`.
It does not modify the general mesh importer's 64-shape limit or original assets.
Its source copies live beside the other probes and in the existing CSSAuthoring
module. The UE module uses its pinned C++20 configuration.

Six candidates are saved:

| Original | Candidate suffix | Morphs |
| --- | --- | --- |
| SK_BlackPearl2 | BlackPearl2 | 22 retained + 99 facial |
| SK_PFit14 | PFit14 | 22 retained + 99 facial |
| SK_SFit16 | SFit16 | 22 retained + 99 facial |
| SK_BFit1 | BFit1 | 22 retained + 99 facial |
| SK_KCloth4 | KCloth4 | 22 retained + 99 facial |
| SK_ACloth3 | ACloth3 | 22 retained + 99 facial |

Fresh-process readback checks all six. Base vertex positions and existing morph
attributes match exactly; retained built facial deltas match the interchange
within 0.000096 cm. The expanded check also covers omitted vertices: Unreal's
unchanged `MorphThresholdPosition` of 0.015 cm filters small deltas. The largest
omission measured on each candidate was 0.015002 cm, within that threshold
plus float reconstruction error. Do not describe the retained-delta error as
the error across all vertices. Material slots, bone bindings, skeletons, physics
assets and post-process graphs remain equal to each original. Verification hashes both
source and candidate files before/after and saves nothing.

Evidence under `work/cine-animation-port/`:

- `facial-export-v1/report.json`: immutable inputs, mapping and export checks.
- `facial-export-v1/facial-shapes.json`: shapes and signed expression weights.
- `facial-export-v1/ue-verification.json`: six saved meshes and readback checks.
- `facial-review-v1/`: 22 Blender head renders, including Reset and all 21 poses.
- `facial-authoring-build.log`, `facial-verify-build.log`: successful UE builds.
- `facial-verify-v3.log`: expanded coverage, including filtered vertices and
  rejection of original assets and duplicate expression additions.

The initial all-six import process was terminated after saving five candidates;
the sixth completed in a separate process. The subsequent fresh-process
verification succeeded for all six. Import progress is now persisted per mesh.

All 22 frontal renders were inspected, including Reset and all 21 expressions.
The poses show distinct brow, cheek, lip and eyelid changes without obvious
mesh tearing in these views. Tease-ya and Zzz close both lids; Wink and Kiss
close opposite lids. Smile and Happy remain distinct, with teeth showing in
Happy. These observations are a solid-material geometry review, not game
lighting or cooked playback acceptance. Side-view teeth/eyelid contacts,
cooked data, runtime ownership/restoration, CINE controls and performance
remain to check.
No expression menu or released outfit update is installed yet.

## Shared facial cook and catalog, 2026-10-06

Six copies now live under `/Game/CSS/Locomotion/Faces/SK_Face_*` in the
authoring project. `stage_facial_library.py` copied only the previously verified
candidates. A fresh process verified geometry, morphs and bindings before the
Windows cook. The original outfit assets and installed containers remain
unchanged. `run_facial_library.py` uses the existing restricted commandlet runner;
the readonly verifier accepts the shared namespace, while the mutation helper
still accepts only the isolated AnimLab namespace.

The cook contains about 311 MiB of skeletal-mesh payloads. The private
`CSS_FacialReadbackProbe_P` container is under the ignored workspace and is not
mounted in the game. Do not distribute or install this diagnostic container.

`tools/FacialReadback` independently decodes the cooked GPU morph buffers using
the pinned CUE4Parse checkout and .NET 10.0.401. All six comparisons passed:

- Reference skeleton, materials, skeleton/physics/post-process bindings and
  sockets match the installed Eve package.
- Render positions, normals, UVs, skin weights, indices, sections and bone lists
  match. Cloth index mappings match; the reader does not decode every cloth buffer.
- All 22 existing body/hand morph delta arrays match exactly after decoding.
- All 99 added facial morphs have nonempty, finite GPU deltas with unique valid
  render-vertex indices. Each mesh has 121 morphs in total.

The cooked position precision is approximately 0.004 cm. This value is not a
measured source-to-cooked error bound. The added facial delta validity check does
not independently compare every delta with the Blender interchange. Editor
source comparisons and cooked GPU checks cover different parts of the pipeline.

Evidence: `work/cine-animation-port/shared-faces-v1/` contains the create/verify
reports, successful cook receipt, private container receipt,
`cooked-readback-v1.json`, and `facial-catalog-evidence.json`.
The catalog generator produces `facial-rig.json`: 21 expressions, 99 morph names,
and six adapters tied to the exact installed Eve container revision.

CSS's animation-library parser now accepts an optional `faces` list. Each rig
declares stable expression IDs, signed weights and exact source-to-derived mesh
bindings. Parsing rejects ambiguous FName casing, non-finite or out-of-range
weights, unknown shapes and conflicting adapters. Library conflicts are rejected
regardless of discovery order. Resolving a different outfit/revision or removing
the pack produces no facial adapter. Old catalogs do not need a `faces` field.

The weight builder clears the rig's other facial shapes on expression changes;
it never adds body or hand controls to that facial list. Reset remains a separate
runtime ownership operation, not the source Neutral expression.

Validation: 22 CSS host suites pass; the real facial catalog passes 40 parser,
compatibility and discovery checks. The decoder builds with zero warnings.
Runtime mesh activation, saved expression selection, CSS/CINE controls and
ownership/restoration are still unfinished. The installed performance trial has
not been replaced by this offline work.

The next data-layer step is implemented: optional `Customization.expression`
stores a stable expression ID in ordinary settings and profile snapshots. Empty
is the original face and is omitted from serialized old/default settings.
Palette changes preserve expression selection. A missing library preserves the
saved identity while `Catalog::facial_match` returns no adapter; an unrecognized
variant cannot borrow another variant's facial mesh. The selection/profile
regressions bring the real-catalog facial checks to 56, with all 22 host suites
still passing. This stores intent only; runtime activation and UI remain next.
The Windows CSS core also builds successfully. Its existing unused `row` warning
in `extension_data.cpp` remains; the expression changes introduced no new warning.
This binary is not installed. The last process check still found the game closed.

Source Neutral has seven nonzero weights. Keep it distinct from Reset, which
must restore the preceding face state while leaving body/hand morphs alone.

## Runtime and combined package checkpoint, 2026-10-06

CSS now plans an expression against the exact outfit revision and variant. It
loads the derived mesh only for a supported selection, validates its skeleton and
declared facial shapes, and retains it through the existing appearance lifetime.
A failed facial asset falls back to the original mesh; that failure is cached
until catalog reload. The saved expression ID survives the fallback.

Facial morph ownership is separate from body/hand morphs. The runtime captures
the original values before writing, rolls back partial failures, and restores
only weights it still owns. A changed or expired mesh is never written through
the old binding. Unchanged weights skip reflected engine calls. Preview and
world components have separate ownership; outfit components do not receive the
facial shape list. Choosing Original face restores the original outfit mesh.

CSS Customize has a searchable Facial Expression picker for supported outfits.
Original face clears the selection; Neutral is the authored nonzero pose.
Palette changes preserve expressions. Reset all customization now explicitly
clears the expression together with the other customization, and still asks
for confirmation. Profiles use the existing optional stable-ID persistence.

The neutral `css.customize` v1 service exposes `expressions`, facial state in
`describe`, and an `expression` command inside `apply`. CINE's expression helper
uses that service. Its menu offers As set in CSS, Original face and supported
expressions. It applies a temporary selection before camera preparation,
waits for CSS to acknowledge it, then restores the previous expression on end
or cancellation. Restoration merges with the current customization rather
than overwriting body/material edits. A changed shell/outfit/variant or a later
expression writer is left alone. Busy cleanup retries outside the take; unload
waits for that cleanup. Look-changing takes require As set in CSS for now.

Validation at this checkpoint:

- Both Windows DLLs build. All 22 CSS host suites and four CINE suites pass.
- The actual facial catalog passes 75 checks, including rollback, signed weights,
  unchanged-write avoidance, saved selection and Reset all.
- CINE passes 34 expression client checks and 231 director checks, covering
  ordinary completion, F9, menu opening, travel, shutdown and failed asset load.
- `shared-v1/pack-with-faces-v1` contains 68 packages: 57 sequences, five playback
  adapters and six facial meshes. All 68 export payloads round-trip exactly.
  Its separate independent facial decoder passes all six meshes again.
  The trio is approximately 315.6 MiB, excluding installed outfit dependencies.

At this checkpoint the installer had not run. The combined candidate was installed
later, as recorded below. Live facial rendering, side contacts, restoration, interaction
with the playback adapters and cold/warm costs remain unverified. Expressions
currently target only the six audited Eve variants. Genessa and Commander White
retain their ordinary faces; their body-animation support is separate.

CINE integration touches files already edited by another session. The exact
additional patch is `work/cine-animation-port/cine-expression-integration.patch`,
with the immediate pre-edit files in `cine-before-expressions/`. Keep those
changes separate from the other session's camera/showreel work when committing.

## Combined private trial installation, 2026-10-06

The combined candidate is now installed with the game closed. The current Windows
builds were checked for pending build work; both were current. The packaged
catalog passed its 83 animation-library checks immediately before installation.
`update_performance_trial.py` verifies the previous trial's selectors, menu, DLLs
and containers before changing anything. It also checks the 68-package readback,
six independent facial readbacks, all 99 shapes and 21 expression comparisons per
mesh, outfit dependencies and protected source evidence.

The installer stages the new files, retains the old DLLs and moves the old shared
pack outside the mounted Paks directory. Five filesystem tests passed: successful
replacement and backup, running-game/changed-file refusal, rollback after a
selector write, rollback between asset moves, and deferred rollback if the game
opens during installation. Rollback never attempts a live DLL replacement.

Installed files:

- CSS core: `css_core-facial-6b9cf837e4c2239c.dll`.
- CINE entry: `cine-facial-bb2d36a5ca792bbf.dll`.
- Shared assets: the verified trio from `shared-v1/pack-with-faces-v1`.
- Receipt and independent post-install check:
  `work/cine-animation-port/live-facial-1/{deployment,post-install-check}.json`.
- Previous trial: `work/cine-animation-port/live-performance-1/deployment.json`.

The independent post-install check confirmed both DLL hashes/selectors, all
three containers, the retained asset rollback, loader and saved CSS/CSSX state.
No outfit container, saved appearance, game input or recording was changed by
installation. The game remained closed. This is a private trial, not a release
or live acceptance. The current user check is Eve Black Pearl's Smile followed
by Original in CSS Customize; it supersedes the performance-only trial prompt.

## Cooked deformation error and side views, 2026-10-06

The independent reader now checks source-to-cooked facial deltas on every
render vertex of all six meshes, rather than reporting compression precision
as if it were measured error. A read-only commandlet exports each verified
candidate's render-to-source vertex map. The decoder verifies that map against
the cooked base positions before comparing the 99 facial shapes. It also sums
the decoded shapes using each of the 21 source expressions' signed weights
and measures the resulting deformation error. No source facial weights were
baked into the neutral base.

Results are identical across the six outfits:

- Mapped base-position difference: zero.
- Maximum retained shape-delta error: 0.0034344653 cm (0.0343 mm).
- Maximum omitted source delta: 0.015001682 cm, within the existing 0.015 cm
  build threshold plus the established 0.0002 cm reconstruction tolerance.
- Largest combined expression deformation error: 0.021667294 cm (0.217 mm),
  in Kiss. Every expression remains inside its weighted per-shape error bound.

Evidence: `work/cine-animation-port/facial-vertex-map-v1/vertex-maps.json`
and `cooked-delta-comparison.json`, using the combined 68-package candidate.
All six source asset hashes remain unchanged. The map follows the imported
vertex correspondence documented by Epic in
[FSkeletalMeshLODModel](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FSkeletalMeshLODModel?application_version=5.6)
and checked against the local UE 5.6.1 header. This is geometry validation;
material appearance and game execution remain separate.

All 21 expressions plus Reset were rendered and inspected from profile and
three-quarter views. Contact sheets are `facial-profile-contact.jpg` and
`facial-quarter-contact.jpg`; full renders are in `facial-review-profile-v1/`
and `facial-review-quarter-v1/`. Kiss's lip silhouette, Surprised/Sob's open
mouth and Tease-ya's closed lids received additional individual inspection.
No obvious tearing or teeth breaking through the lips was visible in these
solid-material views. This does not establish every in-game camera/lighting
condition or animated facial contact.

The .NET reader builds with SDK 10.0.401 and zero warnings/errors. On this
machine `mise exec dotnet@10.0.401 -- dotnet` currently selects a separate
9.0.317 root. Invoke the existing binary at
`/home/eins0fx/.local/share/mise/installs/dotnet/10.0.401/dotnet` explicitly;
its `--version` returns 10.0.401. No SDK or machine configuration was changed.

The negative control changed one index in a separate copy of the exported map.
The independent reader rejected that mesh with a source-position mismatch;
the other five still passed. `negative-comparison.json` records the expected
failure. Neither the valid map nor any source/cooked asset was changed.
`run_facial_library.py --mode map` reproduces the read-only map export into
a fresh workspace beside `shared-faces-v1`.

The implementation follows the pinned local UE 5.6.1
`SkeletalMeshAttributes.h` morph attributes and mesh-description build path.
See also Epic's [morph target API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UMorphTarget?application_version=5.6)
and Blender's [shape-key reference](https://docs.blender.org/api/4.5/bpy.types.ShapeKey.html).

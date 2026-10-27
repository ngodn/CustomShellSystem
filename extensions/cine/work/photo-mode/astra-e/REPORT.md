# Lane E: CINE core content data

Scope: Lane E, Addendum 3 including its appended Addendum 3a. Starting commit is recorded in [base-commit.txt](base-commit.txt). Work is on `main`. No editor, cook, install, game launch, dev-channel request, push, branch change, stash or worktree was used.

CINE now reads fifteen schema-1 rows from `/Game/CSSX/CINE/Core/DT_CINE_Core.DT_CINE_Core`. Each row uses `GameplayTagTableRow`, with JSON in `DevComment`. Scene packs may supply partial rows with the same names. Omitted fields inherit core values; objects merge by field and arrays replace whole lists. A patch may omit `schema` (inherits 1); an explicit schema must be 1. Null is invalid, not deletion. Unknown rows/fields, invalid values and relational errors skip that row with a warning naming the pack and row. Other valid rows still apply. Malformed optional row JSON does not discard the pack's scenes.

For example, a pack row named `style.callouts` may contain `{"schema":1,"label_font":24}`, and `recipe.defaults` may contain `{"duration":7}`. [resolve_content](../../../src/content.cpp) prevalidates and merges each pack at discovery. Presets parse under that pack's defaults, including its guide catalog. Their pack identity uses the folder token, so equal display names cannot mix styles. At take start, the director binds that pack's immutable typed result once and freezes the selected preset. Finish, cancellation, failure and world teardown restore the loaded core content. There is no merge or JSON access per frame. HUD style changes update existing layers without recreating them; a changed FontFace gets a new wrapper and old wrappers can be collected once their text references are replaced. Reload is refused during a take.

The only authored fallback values are [assets/core/content.json](../../../assets/core/content.json). CMake embeds that file directly into the parser, without a second handwritten default table. [schema.json](../../../assets/core/schema.json) defines required fields, types, lengths and numeric bounds. [generate.py](../../../assets/core/generate.py) generates typed structs and readers, without defaults. Changes to values need no type regeneration; schema changes do. The generated-code host check detects stale readers.

[content.cpp](../../../src/content.cpp) validates complete rows, finite values, normalized body weights, layer and guide IDs, asset roots, palette exclusions, reel order, paired bounds and allowlisted operation names. Invalid data is refused, not partly applied. The loader makes one bridge request per advance: load, list, then one row per tick. It publishes only after the whole table passes. It drops its table handle and temporary parsing buffers afterward, retaining the validated core rows for pack merges. The director waits for a ready player, retains the old defaults if the table is missing/old/invalid, logs one warning, and includes that fact in both the menu and CSSX status. Taking a shot waits for startup loading to finish. Subsequent idle ticks do not read content.

Runtime consumers read typed scalar fields, strings and bounded vectors. They do not parse JSON or read a DataTable in frame callbacks. The body envelope has bounded storage allocated while preparing, and missing bones are omitted. Existing subject roles remain protocol IDs, with their bone names and normalized weights supplied by the row. Core values were copied from the current source, including `WallProfile`'s actual 1.0-second hold, rather than assuming every number in the older audit was current.

## B01 to B27 disposition

All row names below refer to [content.json](../../../assets/core/content.json). The matching types and bounds are in [schema.json](../../../assets/core/schema.json); runtime use is in the linked source files. The original evidence remains [Lane B's audit](../astra-b/hardcodes-delegated.md).

| Audit | New home or retained implementation |
| --- | --- |
| B01 | `style.breakdown.collection`, `wire`, `clay`, `xray`, `buffer_post`; [breakdown.cpp](../../../src/breakdown.cpp). Existing main-pak material paths are preserved, so no material relocation or dependency change is required. Material paths must remain under CINE and retain the `_Cine_` ownership marker used by recovery. |
| B02 | `style.breakdown.layers[].color`, keyed by the supported layer ID. Zero colors for non-color passes remain zero. |
| B03 | `style.breakdown.clay_ambient`, `depth_gamma`, `clay_overlay_push`, `bone_glow`, `wire_glow`, `bone_opacity`. |
| B04 | `style.geometry`: `stick_min`, `stick_max`, `radius_per_length`, `radius_min`, `radius_max`, `joint_radius`, `spring_radius`, `socket_radius`, `physics_line`. Engine cone/sphere dimensions stay code. |
| B05 | `style.bones.spring_groups[].prefixes/group`, `virtual_prefix`, `ik_prefix`, `helper_contains`, `helper_prefixes`. Parsed match strings are folded to lowercase. Virtual, IK, explicit/default spring, helper, regular precedence stays interpreter logic. |
| B06 | `style.annotations.wire`, `clay`, `skeleton`, `virtual_bones`, `ik`, `springs`, `sockets`, `physics`, `capsule`, `spring_fallback`; `{count}` and `{group}` are bounded template substitutions. Wire-over-clay precedence remains the renderer's layer conflict rule. |
| B07 | `style.annotations.head`, `torso`, `physics_bones`, `socket_groups[].contains`. Capsule origin remains the capsule operation's anchor. |
| B08 | `style.annotations.preferred_targets`, `spread_targets`; both are bounded to four. Spread, group-root and socket-group algorithms remain code, selected by the existing supported layer operation. Four arrows remains the allocation cap. |
| B09 | `style.breakdown.layers[]` supplies caption order and `label`; `separator` supplies concatenation. Supported layer IDs and masks remain code. |
| B10 | `style.callouts.font_asset`, `font_file`, `face`. The new pak carries an inline `FontFace`; the existing reflected Font wrapper uses its `FontFaceAsset`. An older/missing pak can still use the existing loose font fallback. Engine cache/hint/loading enums remain bindings. |
| B11 | `style.callouts`: four `*_font` and `*_spacing` fields, `line_height`, `shelf_gap`, `uppercase`, `width_em`, `label_padding`, `reference_width`, `scale_min/max`; [callouts.cpp](../../../src/callouts.cpp). Points-to-HUD conversion remains math. |
| B12 | `style.callouts.ink`, `faint`, `trace`, `accent`, `caption_background`. Signature name/small-print role assignment remains layout logic. |
| B13 | `style.callouts.margin`, `row_top/bottom`, `max_row_step`, `max_width`, `split_grace`. Balanced full-frame columns and a fixed split column remain layout algorithms. |
| B14 | `style.callouts.accent_left/right/y/width/inset`, `shelf_width`, `leader_width`, `arrow_length/angle/width`. Shelf centering derives from its width. |
| B15 | `style.callouts.caption_y/height/padding/accent/text_x/text_y/inset/text_height`. |
| B16 | `style.callouts.signature_break/skip/inset/bottom/accent_y/accent_width/accent_inset`, `name_height`, `sign_height`, `signature_lines`. The first line remains the name; six lines remains the hard cap. |
| B17 | `style.guides.catalog[]` with `id`, `label`, `width`, `height`; [guides.cpp](../../../src/guides.cpp). Both menus bind `model.options.guide`; static menu options contain only Off. Legacy enum ordinals remain source compatibility aliases, not a closed catalog. |
| B18 | `style.guides.border_px`, `matte`, `border`, `help_*`, `count_*`. HUD ordering and clipping/visibility guards stay code. |
| B19 | `recipe.body.head_socket`; [rig.cpp](../../../src/rig.cpp). |
| B20 | `recipe.transitions.curve`, `exponent`, `lock_outgoing`. Values remain UE blend enum 4, exponent 2, false. Instant restoration stays code. |
| B21 | `recipe.picture.sharp/full`, in the fixed cvar order in `rig.cpp`: directional rays/samples, local rays/samples, directional/local dither, screen percentage. `-1e9` means leave unchanged. The cvar allowlist, raise-only rule, read-before-write and restore stay code. |
| B22 | `style.materials.root` and the declared breakdown/font object paths. Main-pak asset identity is preserved. Named graph operations and the fixed `DT_CINE_Core` ABI path stay authoring code. |
| B23 | `style.materials.wipe_soft`, `wipe_line`. Authoring uses them and the runtime writes them with each wipe. Initial timing/visibility values remain renderer control state, not scene choreography. |
| B24 | `style.materials.wire_color/glow/push`, `clay_color/ambient/roughness/specular/push`; [author_materials.py](../../../assets/author_materials.py). Shader blend modes, two-sided capability and metallic zero remain material implementation. Runtime overrides remain distinct B02/B03 fields, preserving today's appearance. |
| B25 | `style.materials.xray_color`, `fresnel`, `rim`, `bone_glow`, `opacity`. Solid shares the authored color/glow. Blend/depth modes and zero base reflection remain shader capability/math. |
| B26 | `style.materials.depth_range`, `lighting_scale`, `lighting_floor`, `gain`, `gamma`, `line_color`, `line_glow`. Runtime non-depth Gamma now reads the same `gamma`. Normal encoding and semantic scene-texture/weight dispatch remain shader math. |
| B27 | `style.materials.endpoint_fade` replaces the appearance ramp of 1000 through the `EndpointFade` parameter. Left-to-right viewport traversal and smoothstep remain the supported wipe algorithm, with the original numerical floors. No alternative wipe operation is advertised. |

## Director audit disposition

These are the director rows from [Lane B's main table](../astra-b/REPORT.md), in its original order.

| Audit family | New home or reason it stays |
| --- | --- |
| Ten-step built-in reel | `recipe.reel.steps[].at/layers/wipe`, `wipe_max_fraction`, `name`, `description`. The director constructs the same reveal timeline from data. |
| Cycle palettes and duplicated return timing | `recipe.palettes.exclude`, `start`, `span`, `walk_restore_offset`, `restore_fraction`, `name`, `description`. Both `make_timeline` and director-generated steps use it. The destructive Original-palette ban remains code. |
| Caption prefix and recipe prose | `style.breakdown.caption_prefix`; recipe `name/description` fields feed the menu model. |
| Nineteen body balls | `recipe.body.envelope[].bone/radius`; storage is bounded to 64 and prepared before playback. |
| Head/neck/chest/pelvis/knees/body targets | `recipe.body.targets[].bones/weights` in fixed `Subject` protocol order. The None role is empty; other roles require normalized weights and at most three bones. |
| Gait, follow, collisions, candidate turns | `recipe.motion`: speeds, easing/follow times, wall margins/minimum scales, turn candidates/scoring and split follow settings. Trace budgets, sampling cadence and geometric guards remain code. |
| Walk finale/timing | `recipe.motion.wide_back`, `wide_at`, `wide_blend`, `close_at`, `tail_allowance`, `dolly_until`, `walk_ease`, `arrival_pad`, `key_pad`. |
| Depth and outfit cover | `recipe.transitions.depth_factor/min`, `focus_offset`, `deep_fstop/focus`, `cover`, `wipe_beats`, `hold_beats`, `min_hold`. Per-step authored overrides and restore deadlines stay intact. |
| Default fashion-walk scene | Removed from DLL and static menu. `Pack.default_preset` is an optional validated manifest member; otherwise the first listed preset is used. The first usable pack in discovery order supplies a new user's choice. Saved or explicitly selected IDs survive reload; a missing saved ID is displayed as unavailable rather than silently replaced. Loose-only users fall back to their first available preset. |
| Camera/preset/tempo defaults and kind grammar | `recipe.defaults` supplies camera pose, timing, reference, zoom/FOV, wipe/share, tempo and easing defaults. Shot-kind/ease operations, parser limits and already-authored camera recipes remain code/data respectively. |
| Ease, Hermite and orbit math | Remains code. These are generic algorithms, not authored scene content. |
| Dormant action defaults and punch/shoulder recipe | Also moved, despite the audit's future-content suggestion: `recipe.action` supplies camera defaults, framing, beat timing, punch reductions, shoulder offsets/aim and return/lead behavior. No fight preset was added to a pack. |
| Route sampling/decimation and reachability | Remains capture/geometry policy, as the audit's Keep decision requires. |
| Guide catalog | `style.guides.catalog`; preset frame validation, guide cycling and both menu models use it. |
| Signature text | Optional `style.callouts.signature_lines`; a readable user `signature.txt` overrides it, including an intentionally empty file. Existing line/length caps remain. |
| Performance/library and expression entries | Already discovered through CSS. No selected dance/expression catalog exists to migrate; discovery, sentinels and limits remain. |
| Outfit, palette, controls, physics, idle, montage selections | Already preset data. Existing parsers and cooked preset rows remain unchanged in purpose. No montage library was invented. |
| Audio transport | Already handled by completed lanes. Existing pack SoundWave ID resolution and player behavior remain intact. |
| Preset discovery | Existing folder/DataTable route retained. Manifest default selection was extended; no new pack-discovery mechanism. |
| Ordinary menu text | Kept UI labels, errors, controls and help. Recipe labels/descriptions and guide options now come from the model. Removed stale embedded scene and reel/cycle prose. |
| Limits, retries, finite checks, ownership and restoration | Remain code. The migration adds validation without replacing these safeguards. |

K01-K15 from the delegated audit stay code: layer/material/engine ABI, numerical guard math, allocation and bridge-work caps, restoration, ownership, game bindings, clean-scene safeguards, DLL dispatch and build version/dependency checks. Material path validation preserves the current ownership-name convention rather than allowing paths that recovery cannot recognize.

## Authoring and handoff

`assets/author_materials.py` reads and validates the JSON source before creating assets. It creates `DT_CINE_Core` using the same `DataTableFactory` plus `fill_data_table_from_json_string(..., GameplayTagTableRow)` route as `assets/packs/editor_pack.py`, saves it, exports it, and compares every DevComment's UTF-8 bytes and empty Tag. Source: `assets/packs/editor_pack.py:58-66,89-99` and the [Epic DataTable API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UDataTableFunctionLibrary).

The typeface is imported with `FontFileImportFactory` as `/Game/CSSX/CINE/Core/Fonts/FF_CINE.FF_CINE` and uses Inline loading. UE's FontFace documentation says non-inline faces produce a `.ufont` during cook; Inline keeps the payload in the asset. Sources: [Epic FontFace 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/FontFace?application_version=5.6), [FontFileImportFactory 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/FontFileImportFactory?application_version=5.6), local UE 5.6.1 `Engine/Source/Runtime/SlateCore/Public/Fonts/CompositeFont.h:285-289` (`FontFaceAsset`). The runtime uses that reflected field in the existing Font wrapper. This path has source evidence, not an editor or game test in Lane E.

Material knobs that were graph constants (`Fresnel`, `LightingFloor`, `EndpointFade`) are now parameters with identical defaults. The runtime initializes those and the remaining material appearance parameters once per MID, so a per-pack patch reaches them. The artistic template defaults for color/glow/opacity/ambient remain separate from the deliberate `style.breakdown` layer overrides; layer values take precedence just as before. `style.materials.root` is the authoring output root, not a runtime relocation command. To use custom cooked materials, override the `style.breakdown` object paths and keep their collection references consistent. The core table object path and authoring root are fixed ABI; attempts to relocate the root are refused. `ExponentIn` is the Fresnel input in local UE 5.6.1 `Runtime/Engine/Public/Materials/MaterialExpressionFresnel.h:26` and [Epic's Fresnel guide](https://dev.epicgames.com/documentation/unreal-engine/using-fresnel-in-your-unreal-engine-materials).

The main packer's expected package set now also includes `Core/DT_CINE_Core`, `Core/Fonts/FF_CINE` and the already-authored `Core/Audio/SM_CINE_Render`. Without that change it would reject the queued cook. Original material paths remain unchanged. The font license remains `fonts/OFL.txt`.

Fable's queued job must author in a fresh stage, reload independently, cook, pack, and read back the DataTable and inline font payload. Include two scene packs with different partial overrides, then a core-only scene, to confirm appearance and font restoration. Confirm the saved table has all fifteen rows, every row's schema is 1, and bytes match `content.json` after canonical JSON serialization. Verify FontFace loading/rendering, both guide menus, altered core values, no-pak fallback and a representative sequence/walk/breakdown in game. No live performance, font appearance or cooked-container claim is made here.

## Verification and measurements

Reproduction: run `python3 extensions/cine/work/photo-mode/astra-e/verify.py` from the repository root. [verify.py](verify.py) builds only this lane's host directory, runs all CSSX host suites, builds Windows with the prescribed command, checks the generated readers and JSON source, checks Python syntax, and scans the moved literals. It records exit codes, elapsed time and build warnings in [verification.json](verification.json), and refuses success if source hashes change during the run.

All eight verification commands returned 0; the literal scan returned 1, meaning no matches. All 16 CSSX host suites passed in 2.71 seconds. The content executable passed 4,741 checks. The final host and Windows build logs contain zero warnings. The build uses the project's C++23 setting, GCC 16.2.1 for host tests, and the existing Windows toolchain through `cssx.py`.

The default source is 15,818 bytes (15 rows, 565 leaf values); the validation schema is 32,555 bytes. The built host library is 2,605,400 bytes and the Windows DLL is 2,575,360 bytes. A Release host loop of one million typed `content().motion.walk_speed` reads took 2,373,853 ns (2.374 ms, about 2.37 ns/read). This is one synthetic host measurement, not a game benchmark or an FPS claim. Material setup adds scalar/vector writes during preparation, including two wipe-style scalars per transition; it adds no content parsing or table reads to the frame path.

The parser tests mutate every required field and each populated array element through missing/null/type/range/length cases, plus non-finite numbers and relational failures. They cover complete and partial overrides, whole-array replacement, unknown rows/fields, bad rows falling back independently, two packs with different styles, and return to loaded core. The bridge mock injects a failure at every one of the seventeen startup requests. A successful load takes one asset load, one row-list read and fifteen row reads, at most one request per advance. One hundred subsequent advances make zero requests. Director tests cover readiness, one fallback warning, visible status, missing saved selections and zero idle host calls. HUD tests switch styles twice and assert that colors/fonts update without allocating any additional layers.

[The literal scan](literal-scan.txt) found no matches for [the audited distinctive content strings and removed constant declarations](moved-literals.txt) under `src/`. Shared numeric values, supported operation IDs, engine/property/parameter names, and ordinary UI text remain for the reasons recorded in the tables above. This targeted grep complements the row-by-row source review; it is not a claim that every occurrence of a common number should vanish.

Build and test evidence: [host build](build-final.log), [host suites](tests.log), [content checks](content-checks.log), [Windows build](windows-build.log), [configure](configure.log), [generated readers](generated-check.log), [source validation](source-check.log), [scope check](scope-check.log). [Source hashes](source-hashes.json) identify the tested inputs; [artifact hashes and sizes](artifacts.json) identify the host library and Windows DLL. These binaries were built, not installed. No cook/container, in-game appearance or frame-time measurement was performed.

Protected renderer functions, `render.cpp`, `render.hpp`, and all `photo_parked_` / `photo.step_*` source lines match the starting commit. Reproduction: [verify_scope.py](verify_scope.py); result: [scope-check.json](scope-check.json). Nothing under `extensions/core/src/core/` was edited.

## Cleanup

Deleted 48 lane-owned scratch or intermediate files, totaling 3,705,860 bytes: 33 baseline copies, two temporary migration scripts, two superseded build logs, ten incomplete object files from the interrupted own build, and the old object-cleanup list consolidated into [cleanup.json](cleanup.json). The objects were regenerated. The current build outputs and cited evidence remain. No source preset, music, shipped material, game file, or another lane's work was deleted.

## Six-line summary

1. Moved CINE's audited content into fifteen validated core rows and added partial pack overrides.
2. Wired typed runtime consumers, dynamic guides, manifest defaults and preserved saved selections.
3. Extended main-pak authoring for the core table and inline font; no editor or cook was run.
4. Passed 16/16 host suites and 4,741 content checks; Windows build passed without warnings.
5. Open: Fable's cook/readback and live appearance, menu, fallback and frame-cost checks.
6. Deleted 48 scratch/intermediate files (3,705,860 bytes); protected code was preserved.
